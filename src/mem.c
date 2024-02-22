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
  BASE_ADDRESS_MAX = 0x01000000,
};

// 確保したメインメモリの配列(1～12MB)。
char* mainMemoryPtr;

// メインメモリの終端(+1)アドレス、容量に等しい。
ULong mainMemoryEnd;

// メインメモリの0番地からsupervisorEndまでがスーパーバイザ領域。
ULong supervisorEnd;

// メインメモリを確保する。
bool AllocateMachineMemory(const Settings* settings) {
  mainMemoryEnd = settings->mainMemorySize;
  mainMemoryPtr = calloc(1, settings->mainMemorySize);
  if (mainMemoryPtr == NULL) return false;

  return true;
}

// メインメモリを解放する。
void FreeMachineMemory(void) {
  free(mainMemoryPtr);
  mainMemoryPtr = NULL;
}

// メインメモリをスーパーバイザ領域として設定する。
void SetSupervisorArea(ULong adr) { supervisorEnd = adr; }

// アクセス可能なメモリ範囲を調べる。
//   指定範囲すべてアクセス可能ならtrueを返す。
//   現在のところ読み書きを区別しない。
bool getAccessibleMemoryRange(ULong adr, ULong len, bool super, Span* result) {
  adr = toPhysicalAddress(adr);
  const ULong end = mainMemoryEnd;

  if (adr < end) {
    // 開始アドレスはメインメモリ内。
    if (!super && adr < supervisorEnd) {
      // ユーザーモードでスーパーバイザ領域をアクセスしようとした。
      *result = (Span){NULL, 0};
      return false;
    }
  } else {
    // メインメモリ外は全てアクセス不可能。
    // 実機の仕様としてはGVRAM等は読み書きできるが、run68では実装していない。
    *result = (Span){NULL, 0};
    return false;
  }

  if (len == 0) {
    // アクセス可能な範囲をすべて返す。
    *result = (Span){mainMemoryPtr + adr, end - adr};
    return true;
  }

  if (end < (adr + len)) {
    // 指定範囲の途中までアクセス可能。
    *result = (Span){mainMemoryPtr + adr, end - adr};
    return false;
  }

  // 指定範囲はすべてアクセス可能。
  *result = (Span){mainMemoryPtr + adr, len};
  return true;
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
  throwBusError(mem.length, false);
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

  snprintf(buf, sizeof(buf), "%s($%08x)%sでバスエラーが発生しました。",
           getAddressSpaceName(adr), adr, dir);
  err68(buf);
}
