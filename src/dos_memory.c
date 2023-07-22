// run68x - Human68k CUI Emulator based on run68
// Copyright (C) 2023 TcbnErik
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

static void MfreeAll(ULong psp);
static bool is_valid_memblk(ULong memblk, ULong* nextptr);

// 必要ならアドレスを加算して16バイト境界に整合する
static ULong align_memblk(ULong adrs) {
  return (adrs + (MEMBLK_ALIGN - 1)) & ~(MEMBLK_ALIGN - 1);
}

// 必要ならアドレスを減算して16バイト境界に整合する
static ULong negetive_align_memblk(ULong adrs) {
  return adrs & ~(MEMBLK_ALIGN - 1);
}

static ULong correct_malloc_size(ULong size) {
  return (size > 0x00ffffff) ? 0x00ffffff : size;
}

// DOS _MALLOC, _MALLOC2 共通処理
Long Malloc(UByte mode, ULong size, ULong parent) {
  ULong sizeWithHeader = correct_malloc_size(size) + SIZEOF_MEMBLK;
  ULong maxSize = 0;
  ULong minSize = 0xffffffff;
  ULong cMemblk = 0, cNewblk = 0, cNext = 0;  // 見つけた候補アドレス

  ULong memoryEnd = ReadSuperULong(OSWORK_MEMORY_END);
  ULong memblk = ReadSuperULong(OSWORK_ROOT_PSP);
  ULong next;
  for (; memblk != 0; memblk = next) {
    next = ReadSuperULong(memblk + MEMBLK_NEXT);

    // 新しくメモリブロックを作る場合のヘッダアドレス
    ULong newblk = align_memblk(ReadSuperULong(memblk + MEMBLK_END));

    // この隙間の末尾アドレス
    ULong limit = (next == 0) ? memoryEnd : next;

    ULong capacity = limit - newblk;
    if (capacity < sizeWithHeader) {
      // この隙間には入らない
      if (capacity > maxSize) maxSize = capacity;
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

  if (cNewblk != 0) {
    if (mode == MALLOC_FROM_HIGHER) {
      // 隙間の高位側にメモリブロックを作成する
      ULong limit = (cNext == 0) ? memoryEnd : cNext;
      cNewblk = negetive_align_memblk(limit - sizeWithHeader);
    }

    build_memory_block(cNewblk, cMemblk, parent, cNewblk + sizeWithHeader,
                       cNext);
    return cNewblk + SIZEOF_MEMBLK;
  }

  if (maxSize > SIZEOF_MEMBLK) return 0x81000000 + (maxSize - SIZEOF_MEMBLK);
  return 0x82000000;  // 完全に確保不可
}

// メモリブロックのヘッダを作成する
void build_memory_block(ULong adr, ULong prev, ULong parent, ULong end,
                        ULong next) {
  WriteSuperULong(adr + MEMBLK_PREV, prev);
  WriteSuperULong(adr + MEMBLK_PARENT, parent);
  WriteSuperULong(adr + MEMBLK_END, end);
  WriteSuperULong(adr + MEMBLK_NEXT, next);

  // 前のメモリブロックの「次のメモリブロック」を更新する
  if (prev != 0) WriteSuperULong(prev + MEMBLK_NEXT, adr);

  // 次のメモリブロックの「前のメモリブロック」を更新する
  if (next != 0) WriteSuperULong(next + MEMBLK_PREV, adr);
}

// DOS _MFREE
Long Mfree(ULong adr) {
  if (adr == 0) {
    MfreeAll(psp[nest_cnt]);
    return DOSE_SUCCESS;
  }

  ULong memblk = adr - SIZEOF_MEMBLK;
  if (!is_valid_memblk(memblk, NULL)) return DOSE_ILGMPTR;

  ULong prev = ReadSuperULong(memblk + MEMBLK_PREV);
  // 先頭のメモリブロック(Human68k)は解放できない
  if (prev == 0) return DOSE_ILGMPTR;

  // 前のメモリブロックの「次のメモリブロック」を更新する
  ULong next = ReadSuperULong(memblk + MEMBLK_NEXT);
  WriteSuperULong(prev + MEMBLK_NEXT, next);

  // 次のメモリブロックの「前のメモリブロック」を更新する
  if (next != 0) WriteSuperULong(next + MEMBLK_PREV, prev);

  return DOSE_SUCCESS;
}

// 指定したプロセスが確保したメモリブロックを全て解放する
static void MfreeAll(ULong psp) {
  ULong next, m;
  for (m = ReadSuperULong(OSWORK_ROOT_PSP);; m = next) {
    next = ReadSuperULong(m + MEMBLK_NEXT);

    ULong parent = ReadSuperULong(m + MEMBLK_PARENT);
    if (parent == psp) {
      // 該当するメモリブロックが見つかった
      ULong prev = ReadSuperULong(m + MEMBLK_PREV);
      if (prev == 0) return;

      // 前のメモリブロックの「次のメモリブロック」を更新する
      WriteSuperULong(prev + MEMBLK_NEXT, next);

      // 次のメモリブロックの「前のメモリブロック」を更新する
      if (next != 0) WriteSuperULong(next + MEMBLK_PREV, prev);

      // 今解放したメモリブロックが確保したメモリブロックも全て解放する
      MfreeAll(m);
    }

    if (next == 0) break;
  }
}

// DOS _SETBLOCK
Long Setblock(ULong adr, ULong size) {
  ULong memblk = adr - SIZEOF_MEMBLK;
  ULong sizeWithHeader = correct_malloc_size(size) + SIZEOF_MEMBLK;

  ULong next;
  if (!is_valid_memblk(adr - SIZEOF_MEMBLK, &next)) return DOSE_ILGMPTR;

  ULong limit = (next == 0) ? ReadSuperULong(OSWORK_MEMORY_END) : next;
  ULong capacity = limit - memblk;
  if (capacity >= sizeWithHeader) {
    WriteSuperULong(memblk + MEMBLK_END, memblk + sizeWithHeader);
    return DOSE_SUCCESS;
  }

  // 指定サイズには変更できない
  if (capacity > SIZEOF_MEMBLK) return 0x81000000 + (capacity - SIZEOF_MEMBLK);
  return 0x82000000;
}

// 有効なメモリ管理ポインタか調べる
static bool is_valid_memblk(ULong memblk, ULong* nextptr) {
  if (nextptr != NULL) *nextptr = 0;

  ULong next, m;
  for (m = ReadSuperULong(OSWORK_ROOT_PSP);; m = next) {
    next = ReadSuperULong(m + MEMBLK_NEXT);
    if (m == memblk) break;
    if (next == 0) return false;
  }

  if (nextptr != NULL) *nextptr = next;
  return true;
}
