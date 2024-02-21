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

// メインメモリの配列(1～12MB)
char* mainMemoryPtr;

// メインメモリの終端(+1)アドレス、容量に等しい
static ULong mainMemoryEnd;

// メインメモリの0番地からsupervisorEndまでがスーパーバイザ領域
static ULong supervisorEnd;

// メインメモリを確保する
bool AllocateMachineMemory(size_t main_size) {
  mainMemoryEnd = main_size;
  mainMemoryPtr = calloc(1, main_size);
  if (mainMemoryPtr == NULL) return false;

  return true;
}

// メインメモリを解放する
void FreeMachineMemory(void) {
  free(mainMemoryPtr);
  mainMemoryPtr = NULL;
}

// メインメモリをスーパーバイザ領域として設定する
void SetSupervisorArea(ULong adr) { supervisorEnd = adr; }

static inline ULong ulmin(ULong a, ULong b) { return (a < b) ? a : b; }

// アクセス可能なメモリ範囲を調べる
//   現在のところ読み書きを区別しない。
bool GetAccessibleMemoryRangeSuper(ULong adr, ULong len, MemoryRange* result) {
  const ULong end = mem_aloc;
  adr &= ADDRESS_MASK;

  if (end <= adr) {
    // メインメモリ外は全て読み書き不可能
    // 実機の仕様としてはGVRAM等は読み書きできるが、run68では実装していない
    *result = (MemoryRange){NULL, adr, 0};
    return false;
  }

  // メインメモリ末尾まで読み書き可能
  *result = (MemoryRange){mainMemoryPtr + adr, adr, ulmin(len, end - adr)};
  return true;
}

// 文字列(ASCIIZ)として読み込み可能なメモリか調べ、バッファへのポインタを返す
//   読み込み不可能ならエラー終了する
char* GetStringSuper(ULong adr) {
  const ULong end = mem_aloc;  // メインメモリ外は全て読み込み不可能
  adr &= ADDRESS_MASK;

  ULong n;
  for (n = adr; n < end; ++n) {
    if (mainMemoryPtr[n] == '\0') {
      // メモリ内にNUL文字があれば、文字列は問題なく読み込み可能
      return mainMemoryPtr + adr;
    }
  }

  // メモリ末尾までNUL文字がなければ不正なメモリを参照してバスエラーになる
  throwBusErrorOnRead(n);
}

// メモリに文字列(ASCIIZ)を書き込む
//   書き込み不可能ならエラー終了する
void WriteStringSuper(ULong adr, const char* s) {
  ULong len = strlen(s) + 1;

  MemoryRange mem;
  if (GetWritableMemoryRangeSuper(adr, len, &mem)) {
    // 書き込めるところまで(または可能なら全部)書き込む
    memcpy(mem.bufptr, s, mem.length);
  }

  if (mem.length < len) {
    // 不正なアドレスまで到達したらバスエラー
    throwBusErrorOnWrite(mem.address + mem.length);
  }
}

/*
 　機能：PCの指すメモリから指定されたサイズのイミディエイトデータをゲットし、
 　　　　サイズに応じてPCを進める
 戻り値：データの値
*/
Long imi_get(char size) {
  ULong adr = pc;

  switch (size) {
    case S_BYTE:
      pc += 2;
      return PeekB(adr + 1);  // 1ワード中の下位バイトが実データ
    case S_WORD:
      pc += 2;
      return PeekW(adr);
    default:  // S_LONG
      pc += 4;
      return PeekL(adr);
  }
}

/*
 　機能：読み込みアドレスのチェック
   引数：adr アドレス(ADDRESS_MASK でマスクした値)
 戻り値： true = OK
         false = NGだが、0を読み込んだとみなす
*/
bool mem_red_chk(ULong adr) {
  char message[256];

  if (GVRAM <= adr) {
    if (ini_info.io_through) return false;
    sprintf(message, "I/OポートorROM($%06X)から読み込もうとしました。", adr);
    err68(message);
  }
  if (SR_S_REF() == 0 || mem_aloc <= adr) {
    sprintf(message, "不正アドレス($%06X)からの読み込みです。", adr);
    err68(message);
  }
  return true;
}

/*
 　機能：書き込みアドレスのチェック
   引数：adr アドレス(ADDRESS_MASK でマスクした値)
 戻り値： true = OK
         false = NGだが、何も書き込まずにOKとみなす
*/
bool mem_wrt_chk(ULong adr) {
  char message[256];

  if (GVRAM <= adr) {
    if (ini_info.io_through) return false;
    sprintf(message, "I/OポートorROM($%06X)に書き込もうとしました。", adr);
    err68(message);
  }
  if (SR_S_REF() == 0 || mem_aloc <= adr) {
    sprintf(message, "不正アドレスへの書き込みです($%06X)", adr);
    err68(message);
  }
  return true;
}

/*
 　機能：メモリから指定されたサイズのデータをゲットする
 戻り値：データの値
*/
Long mem_get(ULong adr, char size) {
  adr &= ADDRESS_MASK;
  if (adr < supervisorEnd || mem_aloc <= adr) {
    if (!mem_red_chk(adr)) return 0;
  }

  switch (size) {
    case S_BYTE:
      return PeekB(adr);
    case S_WORD:
      return PeekW(adr);
    default:  // S_LONG
      return PeekL(adr);
  }
}

/*
 　機能：メモリに指定されたサイズのデータをセットする
 戻り値：なし
*/
void mem_set(ULong adr, Long d, char size) {
  adr &= ADDRESS_MASK;
  if (adr < supervisorEnd || mem_aloc <= adr) {
    if (!mem_wrt_chk(adr)) return;
  }

  switch (size) {
    case S_BYTE:
      PokeB(adr, d);
      return;
    case S_WORD:
      PokeW(adr, d);
      return;
    default:  // S_LONG
      PokeL(adr, d);
      return;
  }
}

static const char* getAddressSpaceName(ULong adr) {
  if (adr < mainMemoryEnd) return "メインメモリ(スーパーバイザ領域)";
  if (adr < GVRAM) return "メインメモリ(未搭載)";
  if (adr < TVRAM) return "GVRAM";
  if (adr < IOPORT) return "TVRAM";
  if (adr < CGROM) return "I/Oポート";
  if (adr < IOCSROM) return "CGROM";
  if (adr < 0x01000000) return "IOCS ROM";
  return "不正なアドレス";
}

void throwBusError(ULong adr, bool onWrite) {
  char buf[256];
  const char* dir = onWrite ? "への書き込み" : "からの読み込み";

  snprintf(buf, sizeof(buf), "%s($%08x)%sでバスエラーが発生しました。",
           getAddressSpaceName(adr), adr, dir);
  err68(buf);
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
