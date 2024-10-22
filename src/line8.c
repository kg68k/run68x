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

#include "run68.h"

/*
 　機能：divu命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Divu(char code1, char code2) {
  char mode;
  char src_reg;
  ULong data;
  ULong ans;
  Long waru_l;

  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  int dst_reg = (code1 & 0x0E) >> 1;
  data = rd[dst_reg];

  /* ソースのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_Data, mode, src_reg, S_WORD, &waru_l)) {
    return true;
  }
  UWord waru = (UWord)waru_l;

  if (waru == 0) {
    err68a("０で除算しました", __FILE__, __LINE__);
  }

  CCR_C_OFF();
  ans = data / waru;
  UWord mod = data % waru;
  if (ans > 0xFFFF) {
    CCR_V_ON();
    return false;
  }
  rd[dst_reg] = ((mod << 16) | ans);

  CCR_V_OFF();
  if (ans >= 0x8000) {
    CCR_N_ON();
    CCR_Z_OFF();
  } else {
    CCR_N_OFF();
    if (ans == 0)
      CCR_Z_ON();
    else
      CCR_Z_OFF();
  }
  return false;
}

/*
 　機能：divs命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Divs(char code1, char code2) {
  char mode;
  char src_reg;
  Long data;
  Long ans;
  Long waru_l;

  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  int dst_reg = ((code1 & 0x0E) >> 1);
  data = rd[dst_reg];

  /* ソースのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_Data, mode, src_reg, S_WORD, &waru_l)) {
    return true;
  }
  Word waru = (Word)waru_l;

  if (waru == 0) {
    err68a("０で除算しました", __FILE__, __LINE__);
  }

  CCR_C_OFF();
  ans = data / waru;
  Word mod = data % waru;
  if (ans > 32767 || ans < -32768) {
    CCR_V_ON();
    return false;
  }
  rd[dst_reg] = ((mod << 16) | (ans & 0xFFFF));

  CCR_V_OFF();
  if (ans < 0) {
    CCR_N_ON();
    CCR_Z_OFF();
  } else {
    CCR_N_OFF();
    if (ans == 0)
      CCR_Z_ON();
    else
      CCR_Z_OFF();
  }
  return false;
}

/*
 　機能：or Dn,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Or1(char code1, char code2) {
  char size;
  char mode;
  char src_reg;
  char dst_reg;
  Long data;
  Long src_data;
  Long work_mode;

  size = ((code2 >> 6) & 0x03);
  mode = ((code2 & 0x38) >> 3);
  src_reg = ((code1 & 0x0E) >> 1);
  dst_reg = (code2 & 0x07);

  /* ソースのアドレッシングモードに応じた処理 */
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
                           &data)) {
    return true;
  }

  /* OR演算 */
  data |= src_data;

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_VariableMemory, work_mode, dst_reg, size, data)) {
    return true;
  }

  /* フラグの変化 */
  general_conditions(data, size);

  return false;
}

/*
 　機能：or <ea>,Dn命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Or2(char code1, char code2) {
  char size;
  char mode;
  char src_reg;
  char dst_reg;
  Long src_data;
  Long data;

  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  dst_reg = ((code1 & 0x0E) >> 1);
  size = ((code2 >> 6) & 0x03);

  /* ソースのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_Data, mode, src_reg, size, &src_data)) {
    return true;
  }

  /* デスティネーションのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_All, EA_DD, dst_reg, size, &data)) {
    return true;
  }

  data |= src_data;

  /* デスティネーションのアドレッシングモードに応じた処理 */
  if (set_data_at_ea(EA_All, EA_DD, dst_reg, size, data)) {
    return true;
  }

  /* フラグの変化 */
  general_conditions(data, size);

  return false;
}

static bool Sbcd(UByte code1, UByte code2) {
  int srcReg = code2 & 7;
  int dstReg = (code1 >> 1) & 7;
  int memToMem = code2 & 0x08;

  Long srcVal = 0;
  Long dstVal = 0;

  const int readMode = memToMem ? EA_AIPD : EA_DD;
  if (get_data_at_ea(EA_All, readMode, srcReg, S_BYTE, &srcVal)) return true;
  if (get_data_at_ea(EA_All, readMode, dstReg, S_BYTE, &dstVal)) return true;

  Long result = SubBcd(dstVal, srcVal);

  const int writeMode = memToMem ? EA_AI : EA_DD;
  if (set_data_at_ea(EA_All, writeMode, dstReg, S_BYTE, result)) return true;

  return false;
}

Long SubBcd(Long x, Long y) {
  Long ccrX = CCR_X_REF() ? 1 : 0;
  Long t = (x & 0xff) - (y & 0xff) - ccrX;
  Long result = t;

  if ((x & 0x0f) < ((y & 0x0f) + ccrX)) result = result - 0x10 + 10;
  if (result < 0) {
    if (t < 0) result = result - 0x100 + (10 << 4);
    CCR_X_C_ON();
  } else
    CCR_X_C_OFF();

  result &= 0xff;
  if (result != 0) CCR_Z_OFF();

  if (result & 0x80)
    CCR_N_ON();
  else
    CCR_N_OFF();

  Long a = result - t;
  if (((t ^ result) & (a ^ result)) & 0x80)
    CCR_V_ON();
  else
    CCR_V_OFF();

  return result;
}

/*
 　機能：8ライン命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool line8(char *pc_ptr) {
  char code1, code2;

  code1 = *(pc_ptr++);
  code2 = *pc_ptr;
  pc += 2;

  if ((code2 & 0xC0) == 0xC0) {
    if ((code1 & 0x01) == 0)
      return (Divu(code1, code2));
    else
      return (Divs(code1, code2));
  }
  if (((code1 & 0x01) == 0x01) && ((code2 & 0xF0) == 0))
    return Sbcd(code1, code2);

  if ((code1 & 0x01) == 0x01)
    return (Or1(code1, code2));
  else
    return (Or2(code1, code2));
}

/* $Id: line8.c,v 1.3 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.2  2009/08/05 14:44:33  masamic
 * Some Bug fix, and implemented some instruction
 * Following Modification contributed by TRAP.
 *
 * Fixed Bug: In disassemble.c, shift/rotate as{lr},ls{lr},ro{lr} alway show
 * word size.
 * Modify: enable KEYSNS, register behaiviour of sub ea, Dn.
 * Add: Nbcd, Sbcd.
 *
 * Revision 1.1.1.1  2001/05/23 11:22:07  masamic
 * First imported source code and docs
 *
 * Revision 1.6  1999/12/21  10:08:59  yfujii
 * Uptodate source code from Beppu.
 *
 * Revision 1.5  1999/12/07  12:45:00  yfujii
 * *** empty log message ***
 *
 * Revision 1.5  1999/11/22  03:57:08  yfujii
 * Condition code calculations are rewriten.
 *
 * Revision 1.3  1999/10/20  04:00:59  masamichi
 * Added showing more information about errors.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
