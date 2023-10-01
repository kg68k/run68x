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

#include "mem.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "run68.h"

NORETURN static void read_invalid_memory(ULong adr);

enum {
  GVRAM = 0x00c00000,
};

// メインメモリの0番地からsupervisorEndまでがスーパーバイザ領域
static ULong supervisorEnd;

// メインメモリをスーパーバイザ領域として設定する
void SetSupervisorArea(ULong adr) { supervisorEnd = adr; }

static inline ULong ulmin(ULong a, ULong b) { return (a < b) ? a : b; }

// 書き込み可能なメモリ範囲を調べる
bool GetWritableMemoryRange(ULong adr, ULong len, MemoryRange* result) {
  const ULong end = mem_aloc;
  *result = (MemoryRange){NULL, 0, 0};
  adr &= ADDRESS_MASK;

  if (end <= adr) {
    // メインメモリ外は全て書き込み不可能
    // 実機の仕様としてはGVRAM等は書き込みできるが、run68では実装していない
    return false;
  }

  // メインメモリ末尾まで書き込み可能
  *result = (MemoryRange){prog_ptr + adr, adr, ulmin(len, end - adr)};
  return true;
}

// 文字列(ASCIIZ)として読み込み可能なメモリか調べ、バッファへのポインタを返す
//   読み込み不可能ならエラー終了する
char* GetMemoryBufferString(ULong adr) {
  const ULong end = mem_aloc;  // メインメモリ外は全て読み込み不可能
  adr &= ADDRESS_MASK;

  ULong n;
  for (n = adr; n < end; ++n) {
    if (prog_ptr[n] == '\0') {
      // メモリ内にNUL文字があれば、文字列は問題なく読み込み可能
      return prog_ptr + adr;
    }
  }

  // メモリ末尾までNUL文字がなければ不正なメモリを参照してバスエラーになる
  read_invalid_memory(n);
}

// メモリに文字列(ASCIIZ)を書き込む
//   書き込み不可能ならエラー終了する
void WriteSuperString(ULong adr, const char* s) {
  ULong len = strlen(s) + 1;

  MemoryRange mem;
  if (!GetWritableMemoryRange(adr, len, &mem)) {
    // メモリアドレスが不正
    mem_wrt_chk(adr & ADDRESS_MASK);
    return;  // 戻ってこないはずだが念のため
  }

  // 書き込めるところまで(または可能なら全部)書き込む
  memcpy(mem.bufptr, s, mem.length);

  if (mem.length < len) {
    // 不正なアドレスまで到達したらバスエラー
    mem_wrt_chk((adr & ADDRESS_MASK) + mem.length);
  }
}

/*
 　機能：PCの指すメモリからインデックスレジスタ＋8ビットディスプレースメント
 　　　　の値を得る
 戻り値：その値
*/
Long idx_get(void) {
  char* mem = prog_ptr + pc;

  // Brief Extension Word Format
  //   D/A | REG | REG | REG | W/L | SCALE | SCALE | 0
  //   M68000ではSCALEは無効、最下位ビットが1でもBrief Formatとして解釈される。
  UByte ext = *mem++;
  Byte disp8 = *mem;
  pc += 2;

  int idx_reg = ((ext >> 4) & 0x07);
  Long idx = (ext & 0x80) ? ra[idx_reg] : rd[idx_reg];
  if ((ext & 0x08) == 0) idx = extl((Word)idx);  // Sign-Extended Word

  return idx + extbl(disp8);
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

/*
 機能：異常終了する
*/
void run68_abort(Long adr) {
  printFmt("アドレス：%08X\n", adr);

  close_all_files();

#ifdef TRACE
  int i;
  printf("d0-7=%08lx", rd[0]);
  for (i = 1; i < 8; i++) {
    printf(",%08lx", rd[i]);
  }
  printf("\n");
  printf("a0-7=%08lx", ra[0]);
  for (i = 1; i < 8; i++) {
    printf(",%08lx", ra[i]);
  }
  printf("\n");
  printf("  pc=%08lx    sr=%04x\n", pc, sr);
#endif
  longjmp(jmp_when_abort, 2);
}

static void read_invalid_memory(ULong adr) {
  char buf[256];
  snprintf(buf, sizeof(buf), "不正アドレス($%08x)からの読み込みです。", adr);
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
