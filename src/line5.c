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

#include <stdbool.h>
#include <stdio.h>

#include "operate.h"
#include "run68.h"

/*
 　機能：dbcc命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static int Dbcc(char code1, char code2) {
  int reg = (code2 & 0x07);
  Word disp16 = imi_get_word();
  UWord counter = (rd[reg] & 0xFFFF);

#ifdef TRACE
  printf("trace: dbcc     src=%d PC=%06lX\n", counter, pc - 2);
#endif

  if (get_cond(code1 & 0x0F)) return false;

  counter--;
  rd[reg] = ((rd[reg] & 0xFFFF0000) | counter);
  if (counter != 0xFFFF) {
    pc += extl(disp16) - 2;
  }
  return false;
}

/*
 　機能：scc命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static int Scc(char code1, char code2) {
  char mode;
  char reg;
  Long src_data;

  mode = (code2 & 0x38) >> 3;
  reg = (code2 & 0x07);

  /* 条件よりビットを決める */
  src_data = get_cond((char)(code1 & 0x0F)) ? 0xff : 0x00;

  /* ディスティネーションのアドレッシングモードに応じた処理 */
  if (set_data_at_ea(EA_VariableData, mode, reg, S_BYTE, src_data)) {
    return true;
  }

  return false;
}

/*
 　機能：addq命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static int Addq(char code1, char code2) {
  char size;
  char mode;
  char reg;
  char src_data;
  int work_mode;

  Long dest_data;

  src_data = (code1 & 0x0E) >> 1;
  if (src_data == 0) src_data = 8;
  size = ((code2 >> 6) & 0x03);
  mode = (code2 & 0x38) >> 3;
  reg = (code2 & 0x07);

  if (mode == EA_AD) {
    if (size == S_BYTE) return IllegalInstruction();  // ADDQ.B #Imm,Anは不可

    // アドレスレジスタ直接モードの時のアクセスサイズは必ずロングワードになる
    size = S_LONG;
  }

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_Variable, work_mode, reg, size, &dest_data)) {
    return true;
  }

  /* Add演算 */
  Long result = dest_data + src_data;

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_Variable, work_mode, reg, size, result)) {
    return true;
  }

  // アドレスレジスタ直接の場合はレジスタは変化しない
  if (mode != EA_AD) {
    /* フラグの変化 */
    add_conditions((Long)src_data, dest_data, result, size, true);
  }

  return false;
}

/*
 　機能：subq命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static int Subq(char code1, char code2) {
  char size;
  char mode;
  char reg;
  char src_data;
  int work_mode;
  Long dest_data;

  src_data = (code1 & 0x0E) >> 1;
  if (src_data == 0) src_data = 8;
  size = ((code2 >> 6) & 0x03);
  mode = (code2 & 0x38) >> 3;
  reg = (code2 & 0x07);

  if (mode == EA_AD) {
    if (size == S_BYTE) return IllegalInstruction();  // SUBQ.B #Imm,Anは不可

    // アドレスレジスタ直接モードの時のアクセスサイズは必ずロングワードになる
    size = S_LONG;
  }

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_Variable, work_mode, reg, size, &dest_data)) {
    return true;
  }

  /* Sub演算 */
  Long result = dest_data - src_data;

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_Variable, work_mode, reg, size, result)) {
    return true;
  }

  // アドレスレジスタ直接の場合はレジスタは変化しない
  if (mode != EA_AD) {
    /* フラグの変化 */
    sub_conditions((Long)src_data, dest_data, result, size, true);
  }

  return false;
}

/*
 　機能：5ライン命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool line5(char *pc_ptr) {
  char code1, code2;

  code1 = *(pc_ptr++);
  code2 = *pc_ptr;
  pc += 2;

  if ((code2 & 0xC0) == 0xC0) {
    if ((code2 & 0x38) == 0x08)
      return (Dbcc(code1, code2));
    else
      return (Scc(code1, code2));
  }
  if ((code1 & 0x01) != 0)
    return (Subq(code1, code2));
  else
    return (Addq(code1, code2));
}

/* $Id: line5.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:07  masamic
 * First imported source code and docs
 *
 * Revision 1.6  1999/12/21  10:08:59  yfujii
 * Uptodate source code from Beppu.
 *
 * Revision 1.5  1999/12/07  12:44:27  yfujii
 * *** empty log message ***
 *
 * Revision 1.5  1999/11/22  03:57:08  yfujii
 * Condition code calculations are rewriten.
 *
 * Revision 1.3  1999/10/20  03:55:03  masamichi
 * Added showing more information about errors.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
