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

// pcの指すメモリからイミディエートデータを指定サイズで読み込む。
//   サイズに応じてpcを進める。
Long imi_get(char size) {
  ULong len = (size == S_LONG) ? 4 : 2;
  Span mem = GetReadableMemory(pc, len);
  if (!mem.bufptr) throwBusErrorOnRead(pc + mem.length);

  pc += len;
  switch (size) {
    case S_BYTE:
      return PeekB(mem.bufptr + 1);  // 1ワード中の下位バイトが実データ。
    case S_WORD:
      return PeekW(mem.bufptr);
    default:  // S_LONG
      return PeekL(mem.bufptr);
  }
}

// メモリの値を指定サイズで読み込む。
Long mem_get(ULong adr, char size) {
  Span mem = GetReadableMemory(adr, 1 << size);
  if (!mem.bufptr) throwBusErrorOnRead(adr + mem.length);

  switch (size) {
    case S_BYTE:
      return PeekB(mem.bufptr);
    case S_WORD:
      return PeekW(mem.bufptr);
    default:  // S_LONG
      return PeekL(mem.bufptr);
  }
}

// メモリに値を指定サイズで書き込む。
void mem_set(ULong adr, Long d, char size) {
  Span mem = GetWritableMemory(adr, 1 << size);
  if (!mem.bufptr) throwBusErrorOnWrite(adr + mem.length);

  switch (size) {
    case S_BYTE:
      PokeB(mem.bufptr, d);
      return;
    case S_WORD:
      PokeW(mem.bufptr, d);
      return;
    default:  // S_LONG
      PokeL(mem.bufptr, d);
      return;
  }
}

/* $Id: mem.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:08  masamic
 * First imported source code and docs
 *
 * Revision 1.4  1999/12/07  12:47:22  yfujii
 * *** empty log message ***
 *
 * Revision 1.4  1999/11/29  06:18:06  yfujii
 * Calling CloseHandle instead of fclose when abort().
 *
 * Revision 1.3  1999/11/01  06:23:33  yfujii
 * Some debugging functions are introduced.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
