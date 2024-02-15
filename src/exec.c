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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "run68.h"

/*
 　機能：1命令実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool prog_exec() {
  char *pc_ptr;
  Long adr;
  short save_s;

  /* 上位4ビットで命令を振り分ける */
  pc_ptr = prog_ptr + pc;
  switch (*pc_ptr & 0xF0) {
    case 0x00:
      return (line0(pc_ptr));
    case 0x10:
    case 0x20:
    case 0x30:
      return (line2(pc_ptr));
    case 0x40:
      return (line4(pc_ptr));
    case 0x50:
      return (line5(pc_ptr));
    case 0x60:
      return (line6(pc_ptr));
    case 0x70:
      return (line7(pc_ptr));
    case 0x80:
      return (line8(pc_ptr));
    case 0x90:
      return (line9(pc_ptr));
    case 0xB0:
      return (lineb(pc_ptr));
    case 0xC0:
      return (linec(pc_ptr));
    case 0xD0:
      return (lined(pc_ptr));
    case 0xE0:
      return (linee(pc_ptr));
    case 0xF0:
      return (linef(pc_ptr));
    case 0xA0:
      save_s = SR_S_REF();
      SR_S_ON();
      adr = mem_get(0x28, S_LONG);
      if (adr != HUMAN_WORK) {
        ra[7] -= 4;
        mem_set(ra[7], pc, S_LONG);
        ra[7] -= 2;
        mem_set(ra[7], sr, S_WORD);
        pc = adr;
        return false;
      }
      if (save_s == 0) SR_S_OFF();
      pc += 2;
      err68("A系列割り込みを実行しました");
    default:
      pc += 2;
      err68("おかしな命令を実行しました");
  }
}

/*
 　機能：コンディションが成立しているかどうか調べる
 戻り値： true = 成立
 　　　　false = 不成立
*/
bool get_cond(char cond) {
  switch (cond) {
    case 0x00: /* t */
      return true;
    case 0x02: /* hi */
      if (CCR_C_REF() == 0 && CCR_Z_REF() == 0) return true;
      break;
    case 0x03: /* ls */
      if (CCR_C_REF() != 0 || CCR_Z_REF() != 0) return true;
      break;
    case 0x04: /* cc */
      if (CCR_C_REF() == 0) return true;
      break;
    case 0x05: /* cs */
      if (CCR_C_REF() != 0) return true;
      break;
    case 0x06: /* ne */
      if (CCR_Z_REF() == 0) return true;
      break;
    case 0x07: /* eq */
      if (CCR_Z_REF() != 0) return true;
      break;
    case 0x08: /* vc */
      if (CCR_V_REF() == 0) return true;
      break;
    case 0x09: /* vs */
      if (CCR_V_REF() != 0) return true;
      break;
    case 0x0A: /* pl */
      if (CCR_N_REF() == 0) return true;
      break;
    case 0x0B: /* mi */
      if (CCR_N_REF() != 0) return true;
      break;
    case 0x0C: /* ge */
      if ((CCR_N_REF() != 0 && CCR_V_REF() != 0) ||
          (CCR_N_REF() == 0 && CCR_V_REF() == 0))
        return true;
      break;
    case 0x0D: /* lt */
      if ((CCR_N_REF() != 0 && CCR_V_REF() == 0) ||
          (CCR_N_REF() == 0 && CCR_V_REF() != 0))
        return true;
      break;
    case 0x0E: /* gt */
      if (CCR_Z_REF() == 0 && ((CCR_N_REF() != 0 && CCR_V_REF() != 0) ||
                               (CCR_N_REF() == 0 && CCR_V_REF() == 0)))
        return true;
      break;
    case 0x0F: /* le */
      if (CCR_Z_REF() != 0 || (CCR_N_REF() != 0 && CCR_V_REF() == 0) ||
          (CCR_N_REF() == 0 && CCR_V_REF() != 0))
        return true;
      break;
  }

  return false;
}

static int begin_undefined(const char *s) {
  const char *u = "未定義";
  return memcmp(s, u, strlen(u)) == 0;
}

/*
 　機能：実行時エラーメッセージを表示する
 戻り値：なし
*/
void err68(char *mes) {
  OPBuf_insert(&OP_info);
  printFmt("run68 exec error: %s PC=%06X\n", mes, pc);
  if (begin_undefined(mes)) printFmt("code = %08X\n", mem_get(pc - 4, S_LONG));
  OPBuf_display(10);
  run68_abort(pc);
}

/*
 　機能：実行時エラーメッセージを表示する(その2)
   引数：char*  mes   <in>    メッセージ
         char*  file  <in>    ファイル名
         int    line  <in>    行番号
 戻り値：なし
*/
void err68a(char *mes, char *file, int line) {
  OPBuf_insert(&OP_info);
  printFmt("run68 exec error: %s PC=%06X\n", mes, pc);
  printFmt("\tAt %s:%d\n", file, line);
  if (begin_undefined(mes)) printFmt("code = %08X\n", mem_get(pc - 4, S_LONG));
  OPBuf_display(10);
  run68_abort(pc);
}

/*
   機能：実行時エラーメッセージを表示する(その3)
   引数：char*  mes  <in>    メッセージ
         Long   pc   <in>    プログラムカウンタ
         Long   ppc  <in>    一つ前に実行した命令のプログラムカウンタ
   戻り値：なし
*/
void err68b(char *mes, Long pc, Long ppc) {
  OPBuf_insert(&OP_info);
  printFmt("run68 exec error: %s PC=%06X\n", mes, pc);
  printFmt("PC of previous op code: PC=%06X\n", ppc);
  if (begin_undefined(mes)) printFmt("code = %08X\n", mem_get(pc - 4, S_LONG));
  OPBuf_display(10);
  run68_abort(pc);
}

/*
 　機能：アドレスレジスタをインクリメントする
 戻り値：なし
*/
void inc_ra(int reg, char size) {
  if (reg == 7 && size == S_BYTE) {
    ra[7] += 2;
  } else {
    switch (size) {
      case S_BYTE:
        ra[reg] += 1;
        break;
      case S_WORD:
        ra[reg] += 2;
        break;
      default: /* S_LONG */
        ra[reg] += 4;
        break;
    }
  }
}

/*
 　機能：アドレスレジスタをデクリメントする
 戻り値：なし
*/
void dec_ra(int reg, char size) {
  if (reg == 7 && size == S_BYTE) {
    ra[7] -= 2;
  } else {
    switch (size) {
      case S_BYTE:
        ra[reg] -= 1;
        break;
      case S_WORD:
        ra[reg] -= 2;
        break;
      default: /* S_LONG */
        ra[reg] -= 4;
        break;
    }
  }
}

/*
 　機能：テキストカラーを設定する
 戻り値：なし
*/
void text_color(short c) {
  switch (c) {
    case 0:
      printf("%c[0;30m", 0x1B);
      break;
    case 1:
      printf("%c[0;36m", 0x1B);
      break;
    case 2:
      printf("%c[0;33m", 0x1B);
      break;
    case 3:
      printf("%c[0;37m", 0x1B);
      break;
    case 4:
      printf("%c[0;1;30m", 0x1B);
      break;
    case 5:
      printf("%c[0;1;36m", 0x1B);
      break;
    case 6:
      printf("%c[0;1;33m", 0x1B);
      break;
    case 7:
      printf("%c[0;1;37m", 0x1B);
      break;
    case 8:
      printf("%c[0;30;40m", 0x1B);
      break;
    case 9:
      printf("%c[0;30;46m", 0x1B);
      break;
    case 10:
      printf("%c[0;30;43m", 0x1B);
      break;
    case 11:
      printf("%c[0;30;47m", 0x1B);
      break;
    case 12:
      printf("%c[0;30;1;40m", 0x1B);
      break;
    case 13:
      printf("%c[0;30;1;46m", 0x1B);
      break;
    case 14:
      printf("%c[0;30;1;43m", 0x1B);
      break;
    case 15:
      printf("%c[0;30;1;47m", 0x1B);
      break;
  }
}

/*
   機能：カーソル位置を得る
 戻り値：カーソル位置
*/
Long get_locate() {
  UWord x = 0, y = 0;  // 未対応

  return ((x << 16) | y);
}

/*
   命令情報リングバッファの作業領域
*/
#define MAX_OPBUF 200
static int num_entries;
static int current_p;
static EXEC_INSTRUCTION_INFO entry[MAX_OPBUF];

/*
   機能：
     実行した命令の情報をリングバッファに保存する。
   パラメータ：
     EXEC_INSTRUCTION_INFO  op <in>  命令情報
   戻り値：
     なし。
*/
void OPBuf_insert(const EXEC_INSTRUCTION_INFO *op) {
  if (num_entries < MAX_OPBUF) {
    num_entries++;
  }
  entry[current_p++] = *op;
  if (MAX_OPBUF == current_p) {
    current_p = 0;
  }
}

/*
   機能：
     命令情報リングバッファをクリアする。
   パラメータ；
     なし。
   戻り値：
     なし。
*/
void OPBuf_clear() {
  num_entries = 0;
  current_p = 0;
}

/*
   機能：
     命令情報リングバッファのサイズを取得する。
     パラメータ：
   なし。
     戻り値：
   int  バッファのエントリ数
*/
int OPBuf_numentries() { return num_entries; }

/*
    機能：
      命令情報リングバッファのno番目のエントリを取得する。
    パラメータ：
      int   no  <in>   取り出したいエントリ番号(0が最近のもの)
    戻り値：
      EXEC_INSTRUCTION_INFO*  命令情報へのポインタ
*/
const EXEC_INSTRUCTION_INFO *OPBuf_getentry(int no) {
  int p;
  if (no < 0 || num_entries <= no) return NULL;
  p = current_p - no - 1;
  if (p < 0) {
    p += MAX_OPBUF;
  }
  return &entry[p];
}

/*
    機能：
      命令情報リングバッファの内容を出力する。
    パラメータ：
      int  n   <in>  表示するバッファのエントリ数
    戻り値：
      なし。
*/
void OPBuf_display(int n) {
  int max = OPBuf_numentries();
  int i;
  if (max < n) n = max;
  print(
      "** EXECUTED INSTRUCTION HISTORY **\n"
      "ADDRESS OPCODE                    MNEMONIC\n"
      "-------------------------------------------------------\n");
  for (i = n - 1; 0 <= i; i--) {
    const EXEC_INSTRUCTION_INFO *op;
    Long addr, naddr;
    char *s, hex[64];
    int j;

    op = OPBuf_getentry(i);
    addr = op->pc;
    s = disassemble(addr, &naddr);
    sprintf(hex, "$%06X ", addr);
    while (addr < naddr) {
      char *p = hex + strlen(hex);
      sprintf(p, "%04X ", PeekW(addr));
      addr += 2;
    }
    for (j = strlen(hex); j < 34; j++) {
      hex[j] = ' ';
    }
    hex[j] = '\0';
    printFmt("%s%s\n", hex, s ? s : "\?\?\?\?");
  }
}

/* $Id: exec.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:07  masamic
 * First imported source code and docs
 *
 * Revision 1.9  1999/12/07  12:42:21  yfujii
 * *** empty log message ***
 *
 * Revision 1.9  1999/11/29  06:22:21  yfujii
 * The way of recording instruction history is changed.
 *
 * Revision 1.8  1999/11/01  06:23:33  yfujii
 * Some debugging functions are introduced.
 *
 * Revision 1.7  1999/10/28  06:34:08  masamichi
 * Modified trace behavior
 *
 * Revision 1.6  1999/10/26  02:12:07  yfujii
 * Fixed a bug of displaying code in the wrong byte order.
 *
 * Revision 1.5  1999/10/26  01:31:54  yfujii
 * Execution history and address trap is added.
 *
 * Revision 1.4  1999/10/22  03:23:18  yfujii
 * #include <dos.h> is removed.
 *
 * Revision 1.3  1999/10/20  02:43:59  masamichi
 * Add for showing more information about errors.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
