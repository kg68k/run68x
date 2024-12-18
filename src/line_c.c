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

#include "run68.h"

/*
 　機能：and Dn,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool And1(char code1, char code2) {
  char size;
  char mode;
  char src_reg;
  char dst_reg;
  Long data;
  int work_mode;
  Long src_data;

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

  /* AND演算 */
  data &= src_data;

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
 　機能：and <ea>,Dn命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool And2(char code1, char code2) {
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

  /* AND演算 */
  data &= src_data;

  if (set_data_at_ea(EA_All, EA_DD, dst_reg, size, data)) {
    return true;
  }

  /* フラグの変化 */
  general_conditions(data, size);

  return false;
}

/*
 　機能：exg命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Exg(char code1, char code2) {
  char mode;
  Long tmp;

  mode = ((code2 & 0xF8) >> 3);
  int src_reg = ((code1 & 0x0E) >> 1);
  int dst_reg = (code2 & 0x07);

  switch (mode) {
    case 0x08:
      tmp = rd[src_reg];
      rd[src_reg] = rd[dst_reg];
      rd[dst_reg] = tmp;
      break;
    case 0x09:
      tmp = ra[src_reg];
      ra[src_reg] = ra[dst_reg];
      ra[dst_reg] = tmp;
      break;
    case 0x11:
      tmp = rd[src_reg];
      rd[src_reg] = ra[dst_reg];
      ra[dst_reg] = tmp;
      break;
    default:
      return IllegalInstruction();
  }

#ifdef TRACE
  printf("trace: exg      PC=%06lX\n", pc);
#endif

  return false;
}

/*
 　機能：mulu命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Mulu(char code1, char code2) {
  char src_reg;
  char mode;
  ULong ans;
  Long src_data_l;
#ifdef TRACE
  Long save_pc = pc;
#endif

  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  int dst_reg = ((code1 & 0x0E) >> 1);
  UWord dst_data = (rd[dst_reg] & 0xFFFF);

  /* ソースのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_Data, mode, src_reg, S_WORD, &src_data_l)) {
    return true;
  }
  UWord src_data = (UWord)src_data_l;

  ans = src_data * dst_data;
  rd[dst_reg] = ans;
#ifdef TRACE
  printf("trace: mulu     src=%u PC=%06lX\n", src_data, save_pc);
#endif

  /* フラグの変化 */
  general_conditions(ans, S_LONG);

  return false;
}

/*
 　機能：muls命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Muls(char code1, char code2) {
  char src_reg;
  char mode;
  Long ans;
  Long src_data_l;
#ifdef TRACE
  Long save_pc = pc;
#endif

  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  int dst_reg = ((code1 & 0x0E) >> 1);

  Word dst_data = (rd[dst_reg] & 0xFFFF);

  /* ソースのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_Data, mode, src_reg, S_WORD, &src_data_l)) {
    return true;
  }
  Word src_data = (Word)src_data_l;

  ans = src_data * dst_data;
  rd[dst_reg] = ans;

#ifdef TRACE
  printf("trace: muls     src=%d PC=%06lX\n", src_data, save_pc);
#endif

  /* フラグの変化 */
  general_conditions(ans, S_LONG);

  return false;
}

// BCD加算
// Makoto Kamada氏のXEiJ (https://stdkmd.net/xeij/)
//   MC68000.javaを参考にしています。
static Long AddBcd(Long x, Long y) {
  Long ccrX = CCR_X_REF() ? 1 : 0;
  Long t = (x & 0xff) + (y & 0xff) + ccrX;
  Long result = t;

  if (10 <= ((x & 0x0f) + (y & 0x0f) + ccrX)) result = result + 0x10 - 10;
  if ((10 << 4) <= result) {
    result = result + 0x100 - (10 << 4);
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

static bool Abcd(UByte code1, UByte code2) {
  int srcReg = code2 & 7;
  int dstReg = (code1 >> 1) & 7;
  int memToMem = code2 & 0x08;

  Long srcVal = 0;
  Long dstVal = 0;

  const int readMode = memToMem ? EA_AIPD : EA_DD;
  if (get_data_at_ea(EA_All, readMode, srcReg, S_BYTE, &srcVal)) return true;
  if (get_data_at_ea(EA_All, readMode, dstReg, S_BYTE, &dstVal)) return true;

  Long result = AddBcd(dstVal, srcVal);

  const int writeMode = memToMem ? EA_AI : EA_DD;
  if (set_data_at_ea(EA_All, writeMode, dstReg, S_BYTE, result)) return true;

  return false;
}

/*
 　機能：Cライン命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool linec(char *pc_ptr) {
  char code1 = *(pc_ptr++);
  char code2 = *pc_ptr;
  pc += 2;

  if ((code1 & 0x01) == 0) {
    if ((code2 & 0xC0) == 0xC0) return Mulu(code1, code2);
    return And2(code1, code2);
  }
  if ((code2 & 0xC0) == 0xC0) return Muls(code1, code2);
  if ((code2 & 0xF0) == 0x00) return Abcd(code1, code2);
  if ((code2 & 0x30) == 0x00) return Exg(code1, code2);
  return (And1(code1, code2));
}

/* $Id: linec.c,v 1.3 2009-08-08 06:49:44 masamic Exp $ */

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
 * Revision 1.1.1.1  2001/05/23 11:22:08  masamic
 * First imported source code and docs
 *
 * Revision 1.7  1999/12/21  10:08:59  yfujii
 * Uptodate source code from Beppu.
 *
 * Revision 1.6  1999/12/07  12:45:42  yfujii
 * *** empty log message ***
 *
 * Revision 1.6  1999/11/22  03:57:08  yfujii
 * Condition code calculations are rewriten.
 *
 * Revision 1.4  1999/11/01  12:10:21  masamichi
 * Maybe correct error at code: $C073
 *
 * Revision 1.3  1999/10/20  04:14:48  masamichi
 * Added showing more information about errors.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
