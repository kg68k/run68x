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

#include "mem.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "run68.h"

enum {
  GVRAM = 0x00c00000,
  TVRAM = 0x00e00000,
  IOPORT = 0x00e80000,
  CGROM = 0x00f00000,
  IOCSROM = 0x00fc0000,
};

char* mainMemoryPtr;  // 確保したメインメモリの配列
char* highMemoryPtr;  // 確保したハイメモリの配列
ULong mainMemoryEnd;  // メインメモリの終端(+1)アドレス、容量に等しい
ULong highMemoryEnd;  // ハイメモリの終端(+1)アドレス
ULong supervisorEnd;  // $0～supervisorEndがスーパーバイザ領域

// メインメモリ、ハイメモリを確保する。
bool AllocateMachineMemory(const Settings* settings, ULong* outHimemAddress) {
  *outHimemAddress = 0;
  highMemoryEnd = 0;
  highMemoryPtr = NULL;

  mainMemoryEnd = settings->mainMemorySize;
  mainMemoryPtr = calloc(1, settings->mainMemorySize);

  ULong himemSize = settings->highMemorySize;
  if (himemSize) {
    *outHimemAddress = HIMEM_START;
    highMemoryEnd = HIMEM_START + himemSize;
    highMemoryPtr = calloc(1, himemSize);
  }

  if (!mainMemoryPtr || (himemSize && !highMemoryPtr)) {
    FreeMachineMemory();
    return false;
  }
  return true;
}

// メインメモリ、ハイメモリを解放する。
void FreeMachineMemory(void) {
  free(mainMemoryPtr);
  mainMemoryPtr = NULL;

  free(highMemoryPtr);
  highMemoryPtr = NULL;
}

// メインメモリをスーパーバイザ領域として設定する。
void SetSupervisorArea(ULong adr) { supervisorEnd = adr; }

// アクセス可能なメモリ範囲を調べる。
//   指定範囲の先頭部分がアクセス可能ならtrueを返す。
//   現在のところ読み書きを区別しない。
bool getAccessibleMemoryRange(ULong adr, ULong len, bool super, Span* result) {
  adr = ToPhysicalAddress(adr);

  ULong end = mainMemoryEnd;
  if (adr < end) {
    // 開始アドレスはメインメモリ内。
    if (!super && adr < supervisorEnd) {
      // ユーザーモードでスーパーバイザ領域をアクセスしようとした。
      *result = (Span){NULL, 0};
      return false;
    }
    ULong max = end - adr;  // 開始アドレスからメモリが実装されている長さ
    if (len == 0 || max < len) {
      // len==0 ... アクセス可能な範囲をすべて返す。
      // len!=0 ... 指定範囲の途中までアクセス可能。
      *result = (Span){mainMemoryPtr + adr, max};
      return true;
    }
    // 指定範囲はすべてアクセス可能。
    *result = (Span){mainMemoryPtr + adr, len};
    return true;
  }

  if (adr < HIMEM_START) {
    // メインメモリ未搭載領域と、$00c00000-$00ffffffはすべてアクセス不可能。
    // 実機の仕様としてはGVRAM等は読み書きできるが、run68では実装していない。
    *result = (Span){NULL, 0};
    return false;
  }

  end = highMemoryEnd;
  if (adr < end) {
    // 開始アドレスはハイメモリ内。
    ULong max = end - adr;
    if (len == 0 || max < len) {
      *result = (Span){highMemoryPtr + (adr - HIMEM_START), max};
      return true;
    }
    // 指定範囲はすべてアクセス可能。
    *result = (Span){highMemoryPtr + (adr - HIMEM_START), len};
    return true;
  }

  // ハイメモリ未搭載領域(実機と異なると思われるが、とりあえず仕様とする)
  *result = (Span){NULL, 0};
  return false;
}

// 文字列(ASCIIZ)として読み込み可能なメモリか調べ、バッファへのポインタを返す。
//   読み込み不可能ならエラー終了する。
char* GetStringSuper(ULong adr) {
  Span mem;
  if (GetReadableMemoryRangeSuper(adr, 0, &mem)) {
    if (memchr(mem.bufptr, '\0', mem.length) != NULL) {
      // メモリ内にNUL文字があれば、文字列は問題なく読み込み可能。
      return mem.bufptr;
    }
  }

  // メモリ末尾までNUL文字がなければ不正なメモリを参照してバスエラーになる。
  throwBusError(adr + mem.length, false);
}

// メモリに文字列(ASCIIZ)を書き込む。
//   書き込み不可能ならエラー終了する。
void WriteStringSuper(ULong adr, const char* s) {
  ULong len = strlen(s) + 1;

  Span mem;
  GetWritableMemoryRangeSuper(adr, len, &mem);
  if (mem.length) {
    // 書き込めるところまで(または可能なら全部)書き込む。
    memcpy(mem.bufptr, s, mem.length);
  }

  if (mem.length < len) {
    // 不正なアドレスまで到達したらバスエラー。
    throwBusErrorOnWrite(adr + mem.length);
  }
}

static const char* getAddressSpaceName(ULong adr) {
  if (adr < mainMemoryEnd) return "メインメモリ(スーパーバイザ領域)";
  if (adr < GVRAM) return "メインメモリ(未搭載)";
  if (adr < TVRAM) return "GVRAM";
  if (adr < IOPORT) return "TVRAM";
  if (adr < CGROM) return "I/Oポート";
  if (adr < IOCSROM) return "CGROM";
  if (adr < BASE_ADDRESS_MAX) return "IOCS ROM";
  return "不正なアドレス";
}

void throwBusError(ULong adr, bool onWrite) {
  char buf[256];
  const char* dir = onWrite ? "への書き込み" : "からの読み込み";
  const char* name = getAddressSpaceName(ToPhysicalAddress(adr));

  snprintf(buf, sizeof(buf), "%s($%08x)%sでバスエラーが発生しました。", name,
           adr, dir);
  err68(buf);
}
