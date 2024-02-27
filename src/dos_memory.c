// run68x - Human68k CUI Emulator based on run68
// Copyright (C) 2024 TcbnErik
//
// This program is free software; you can redistribute it and /or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301 USA.

#include "dos_memory.h"

#include "human68k.h"
#include "mem.h"
#include "run68.h"

#define MALLOC_MAX_SIZE 0x00fffff0
#define MALLOC3_MAX_SIZE 0x7ffffff0

// DOS _MALLOCで確保する対象のメモリ空間。
static AllocArea allocArea = ALLOC_AREA_MAIN_ONLY;

static ULong tryMalloc(UByte mode, ULong size, ULong parent, ULong* maxSize);
static void MfreeAll(ULong psp);
static bool is_valid_memblk(ULong memblk, ULong* nextptr);

// 確保するメモリ空間を指定する。
void SetAllocArea(AllocArea area) { allocArea = area; }

// 確保しようとしているアドレスが指定したメモリ空間か調べる。
static bool isAllocatableArea(ULong adr) {
  switch (allocArea) {
    case ALLOC_AREA_MAIN_ONLY:
      return (adr < BASE_ADDRESS_MAX);
    case ALLOC_AREA_HIGH_ONLY:
      return (adr >= BASE_ADDRESS_MAX);

    default:
    case ALLOC_AREA_UNLIMITED:
      return true;
  }
}

// 必要ならアドレスを加算して16バイト境界に整合する
static ULong align_memblk(ULong adrs) {
  return (adrs + (MEMBLK_ALIGN - 1)) & ~(MEMBLK_ALIGN - 1);
}

// 必要ならアドレスを減算して16バイト境界に整合する
static ULong negetive_align_memblk(ULong adrs) {
  return adrs & ~(MEMBLK_ALIGN - 1);
}

// 最大サイズでメモリブロックを確保する。
MallocResult MallocAll(ULong parent) {
  const UByte mode = MALLOC_FROM_LOWER;
  ULong size = Malloc(MALLOC_FROM_LOWER, (ULong)-1, parent) & MALLOC_MASK;
  Long adr = Malloc(MALLOC_FROM_LOWER, size, parent);
  return (MallocResult){adr, size};
}

static ULong determineRequestSize(ULong size) {
  if (size > MALLOC_MAX_SIZE)
    return (ULong)-1;  // 最大サイズの取得なら絶対に確保できないサイズを返す。

  return size + SIZEOF_MEMBLK;
}

// DOS _MALLOC、_MALLOC2 共通処理
Long Malloc(UByte mode, ULong size, ULong parent) {
  ULong sizeWithHeader = determineRequestSize(size);
  ULong maxSize;
  ULong adr = tryMalloc(mode, sizeWithHeader, parent, &maxSize);
  if (adr) return adr;

  if (maxSize <= SIZEOF_MEMBLK) return DOSE_MALLOC_NOMEM2;

  maxSize -= SIZEOF_MEMBLK;
  ULong n = (maxSize <= MALLOC_MAX_SIZE) ? maxSize : MALLOC_MAX_SIZE;
  return DOSE_MALLOC_NOMEM | n;
}

static ULong determineRequestSizeHuge(ULong size) {
  if (size > MALLOC3_MAX_SIZE)
    return (ULong)-1;  // 最大サイズの取得なら絶対に確保できないサイズを返す。

  return size + SIZEOF_MEMBLK;
}

// DOS _MALLOC3、_MALLOC4 共通処理
Long MallocHuge(UByte mode, ULong size, ULong parent) {
  ULong sizeWithHeader = determineRequestSizeHuge(size);
  ULong maxSize;
  ULong adr = tryMalloc(mode, sizeWithHeader, parent, &maxSize);
  if (adr) return adr;

  ULong n = (maxSize <= SIZEOF_MEMBLK) ? 0 : maxSize - SIZEOF_MEMBLK;
  return DOSE_MALLOC3_NOMEM | n;
}

// メモリブロック確保共通処理
static ULong tryMalloc(UByte mode, ULong sizeWithHeader, ULong parent,
                       ULong* outMaxSize) {
  ULong minSize = (ULong)-1;
  ULong cMemblk = 0, cNewblk = 0, cNext = 0;  // 見つけた候補アドレス
  *outMaxSize = 0;  // 確保可能な最大サイズ(確保できなかった場合のみ)

  ULong memoryEnd = ReadULongSuper(OSWORK_MEMORY_END);
  ULong memblk = ReadULongSuper(OSWORK_ROOT_PSP);
  ULong next;
  for (; memblk != 0; memblk = next) {
    next = ReadULongSuper(memblk + MEMBLK_NEXT);

    // 新しくメモリブロックを作る場合のヘッダアドレス
    ULong newblk = align_memblk(ReadULongSuper(memblk + MEMBLK_END));
    if (!isAllocatableArea(newblk)) continue;

    // この隙間の末尾アドレス
    ULong limit = next ? next : memoryEnd;

    ULong capacity = limit - newblk;
    if (capacity < sizeWithHeader) {
      // この隙間には入らない
      if (capacity > *outMaxSize) *outMaxSize = capacity;
      continue;
    }

    // メモリブロックを作れる隙間が見つかった
    if (mode == MALLOC_FROM_LOWER) {
      // MALLOC_FROM_LOWER なら直ちに確定
      cMemblk = memblk;
      cNewblk = newblk;
      cNext = next;
      break;
    }
    if (mode == MALLOC_FROM_SMALLEST) {
      // この隙間の方が大きければ不採用
      if (minSize <= capacity) continue;
      minSize = capacity;
    }
    // MALLOC_FROM_SMALLEST でこの隙間の方が小さい、または MALLOC_FROM_HIGHER
    // なら暫定候補とし、残りのメモリブロックについても調べる
    cMemblk = memblk;
    cNewblk = newblk;
    cNext = next;
  }

  if (cNewblk == 0) return 0;  // 確保できない。

  // メモリブロック作成。
  if (mode == MALLOC_FROM_HIGHER) {
    // 隙間の高位側にメモリブロックを作成する
    ULong limit = cNext ? cNext : memoryEnd;
    cNewblk = negetive_align_memblk(limit - sizeWithHeader);
  }

  BuildMemoryBlock(cNewblk, cMemblk, parent, cNewblk + sizeWithHeader, cNext);
  return cNewblk + SIZEOF_MEMBLK;
}

// メモリブロックのヘッダを作成する
void BuildMemoryBlock(ULong adr, ULong prev, ULong parent, ULong end,
                      ULong next) {
  WriteULongSuper(adr + MEMBLK_PREV, prev);
  WriteULongSuper(adr + MEMBLK_PARENT, parent);
  WriteULongSuper(adr + MEMBLK_END, end);
  WriteULongSuper(adr + MEMBLK_NEXT, next);

  // 前のメモリブロックの「次のメモリブロック」を更新する
  if (prev != 0) WriteULongSuper(prev + MEMBLK_NEXT, adr);

  // 次のメモリブロックの「前のメモリブロック」を更新する
  if (next != 0) WriteULongSuper(next + MEMBLK_PREV, adr);
}

// DOS _MFREE
Long Mfree(ULong adr) {
  if (adr == 0) {
    MfreeAll(psp[nest_cnt]);
    return DOSE_SUCCESS;
  }

  ULong memblk = adr - SIZEOF_MEMBLK;
  if (!is_valid_memblk(memblk, NULL)) return DOSE_ILGMPTR;

  ULong prev = ReadULongSuper(memblk + MEMBLK_PREV);
  // 先頭のメモリブロック(Human68k)は解放できない
  if (prev == 0) return DOSE_ILGMPTR;

  // 前のメモリブロックの「次のメモリブロック」を更新する
  ULong next = ReadULongSuper(memblk + MEMBLK_NEXT);
  WriteULongSuper(prev + MEMBLK_NEXT, next);

  // 次のメモリブロックの「前のメモリブロック」を更新する
  if (next != 0) WriteULongSuper(next + MEMBLK_PREV, prev);

  return DOSE_SUCCESS;
}

// 指定したプロセスが確保したメモリブロックを全て解放する
static void MfreeAll(ULong psp) {
  ULong next, m;
  for (m = ReadULongSuper(OSWORK_ROOT_PSP);; m = next) {
    next = ReadULongSuper(m + MEMBLK_NEXT);

    ULong parent = ReadULongSuper(m + MEMBLK_PARENT);
    if (parent == psp) {
      // 該当するメモリブロックが見つかった
      ULong prev = ReadULongSuper(m + MEMBLK_PREV);
      if (prev == 0) return;

      // 前のメモリブロックの「次のメモリブロック」を更新する
      WriteULongSuper(prev + MEMBLK_NEXT, next);

      // 次のメモリブロックの「前のメモリブロック」を更新する
      if (next != 0) WriteULongSuper(next + MEMBLK_PREV, prev);

      // 今解放したメモリブロックが確保したメモリブロックも全て解放する
      MfreeAll(m);
    }

    if (next == 0) break;
  }
}

// DOS _SETBLOCK
Long Setblock(ULong adr, ULong size) {
  ULong sizeWithHeader = determineRequestSize(size);

  ULong memblk = adr - SIZEOF_MEMBLK;
  ULong next;
  if (!is_valid_memblk(memblk, &next)) return DOSE_ILGMPTR;

  ULong limit = next ? next : ReadULongSuper(OSWORK_MEMORY_END);
  ULong maxSize = limit - memblk;

  if (maxSize < sizeWithHeader) {
    // 指定サイズに変更できるだけの隙間がない。
    if (maxSize <= SIZEOF_MEMBLK) return DOSE_MALLOC_NOMEM2;

    maxSize -= SIZEOF_MEMBLK;
    ULong n = (maxSize <= MALLOC_MAX_SIZE) ? maxSize : MALLOC_MAX_SIZE;
    return DOSE_MALLOC_NOMEM | n;
  }

  // サイズ変更。
  WriteULongSuper(memblk + MEMBLK_END, adr + size);
  return DOSE_SUCCESS;
}

// DOS _SETBLOCK2
Long SetblockHuge(ULong adr, ULong size) {
  ULong sizeWithHeader = determineRequestSizeHuge(size);

  ULong memblk = adr - SIZEOF_MEMBLK;
  ULong next;
  if (!is_valid_memblk(memblk, &next)) return DOSE_ILGMPTR;

  ULong limit = next ? next : ReadULongSuper(OSWORK_MEMORY_END);
  ULong maxSize = limit - memblk;

  if (maxSize < sizeWithHeader) {
    // 指定サイズに変更できるだけの隙間がない。
    ULong n = (maxSize <= SIZEOF_MEMBLK) ? 0 : maxSize - SIZEOF_MEMBLK;
    return DOSE_MALLOC3_NOMEM | n;
  }

  // サイズ変更。
  WriteULongSuper(memblk + MEMBLK_END, adr + size);
  return DOSE_SUCCESS;
}

// 有効なメモリ管理ポインタか調べる
static bool is_valid_memblk(ULong memblk, ULong* nextptr) {
  if (nextptr != NULL) *nextptr = 0;

  ULong next, m;
  for (m = ReadULongSuper(OSWORK_ROOT_PSP);; m = next) {
    next = ReadULongSuper(m + MEMBLK_NEXT);
    if (m == memblk) break;
    if (next == 0) return false;
  }

  if (nextptr != NULL) *nextptr = next;
  return true;
}
