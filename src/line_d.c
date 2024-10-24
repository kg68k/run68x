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

static bool Adda(char code1, char code2) {
  char mode;
  char src_reg;
  char size;
  Long src_data;
#ifdef TRACE
  Long save_pc = pc;
#endif

  int dst_reg = ((code1 & 0x0E) >> 1);
  if ((code1 & 0x01) == 0x01)
    size = S_LONG;
  else
    size = S_WORD;
  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);

  /* ソースのアドレッシングモードに応じた処理 */
  if (size == S_BYTE) return IllegalInstruction();  // ADDA.B <ea>,Anは不可
  if (get_data_at_ea(EA_All, mode, src_reg, size, &src_data)) return true;

  if (size == S_WORD) {
    if ((src_data & 0x8000) != 0) {
      src_data |= 0xFFFF0000;
    } else {
      src_data &= 0x0000FFFF;
    }
  }

  ra[dst_reg] += src_data;

#ifdef TRACE
  printf("trace: adda.%c   src=%d PC=%06lX\n", size_char[size], src_data,
         save_pc);
#endif

  return false;
}

// ADDX -(Ay),-(Ax)
static bool AddxMem(char code1, char code2) {
  int src_reg = (code2 & 0x07);
  int dst_reg = ((code1 & 0x0E) >> 1);
  char size = ((code2 >> 6) & 0x03);

  Long src_data, dst_data;
  if (get_data_at_ea(EA_All, EA_AIPD, src_reg, size, &src_data) ||
      get_data_at_ea(EA_All, EA_AIPD, dst_reg, size, &dst_data)) {
    return true;
  }

  bool save_z = CCR_Z_REF() != 0 ? true : false;
  Long result = dst_data + src_data + (CCR_X_REF() ? 1 : 0);
  if (set_data_at_ea(EA_All, EA_AI, dst_reg, size, result)) {
    return true;
  }
  add_conditions(src_data, dst_data, result, size, save_z);

  return false;
}

static bool Addx(char code1, char code2) {
  int src_reg = (code2 & 0x07);
  int dst_reg = ((code1 & 0x0E) >> 1);
  char size = ((code2 >> 6) & 0x03);

  Long src_data = rd[src_reg];
  Long dst_data = rd[dst_reg];

  bool save_z = CCR_Z_REF() != 0 ? true : false;
  Long result = dst_data + src_data + (CCR_X_REF() ? 1 : 0);
  SetDreg(dst_reg, result, size);
  add_conditions(src_data, dst_data, result, size, save_z);

  return false;
}

/*
 　機能：add Dn,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Add1(char code1, char code2) {
  char size;
  char mode;
  char src_reg;
  char dst_reg;
  int work_mode;
  Long src_data;
  Long dest_data;

  mode = ((code2 & 0x38) >> 3);
  src_reg = ((code1 & 0x0E) >> 1);
  dst_reg = (code2 & 0x07);
  size = ((code2 >> 6) & 0x03);

  if (get_data_at_ea(EA_All, EA_DD, src_reg, size, &src_data)) {
    return true;
  }

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableMemory, work_mode, dst_reg, size,
                           &dest_data)) {
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

  if (set_data_at_ea(EA_VariableMemory, work_mode, dst_reg, size, result)) {
    return true;
  }

  /* フラグの変化 */
  sub_conditions(src_data, dest_data, result, size, true);

  return false;
}

/*
 　機能：add <ea>,Dn命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Add2(char code1, char code2) {
  char size;
  char mode;
  char src_reg;
  Long src_data;
  Long dest_data;
#ifdef TRACE
  Long save_pc = pc;
#endif

  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  int dst_reg = ((code1 & 0x0E) >> 1);
  size = ((code2 >> 6) & 0x03);

  if (mode == EA_AD && size == S_BYTE)
    return IllegalInstruction();  // ADD.b An,Dnは不可
  if (get_data_at_ea(EA_All, mode, src_reg, size, &src_data)) return true;

  if (get_data_at_ea(EA_All, EA_DD, dst_reg, size, &dest_data)) {
    return true;
  }

  SetDreg(dst_reg, dest_data + src_data, size);

  /* フラグの変化 */
  add_conditions(src_data, dest_data, rd[dst_reg], size, true);

#ifdef TRACE
  printf("trace: add.%c    src=%d PC=%06lX\n", size_char[size], src_data,
         save_pc);
#endif

  return false;
}

/*
 　機能：Dライン命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool lined(char *pc_ptr) {
  char code1 = *(pc_ptr++);
  char code2 = *pc_ptr;
  pc += 2;

  if ((code2 & 0xC0) == 0xC0) return Adda(code1, code2);

  if ((code1 & 0x01) == 1) {
    if ((code2 & 0x30) == 0x00) {
      return (code2 & 0x08) ? AddxMem(code1, code2) : Addx(code1, code2);
    }
    return Add1(code1, code2);
  }

  return Add2(code1, code2);
}

/* $Id: lined.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:08  masamic
 * First imported source code and docs
 *
 * Revision 1.5  1999/12/21  10:08:59  yfujii
 * Uptodate source code from Beppu.
 *
 * Revision 1.4  1999/12/07  12:45:54  yfujii
 * *** empty log message ***
 *
 * Revision 1.4  1999/11/22  03:57:08  yfujii
 * Condition code calculations are rewriten.
 *
 * Revision 1.3  1999/10/20  04:14:48  masamichi
 * Added showing more information about errors.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
