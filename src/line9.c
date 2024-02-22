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

static bool Suba(char code1, char code2) {
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
  if (size == S_BYTE) {
    err68a("不正な命令: suba.b <ea>, An を実行しようとしました。", __FILE__,
           __LINE__);
  } else if (get_data_at_ea(EA_All, mode, src_reg, size, &src_data)) {
    return true;
  }

  if (size == S_WORD) src_data = extl(src_data);

  // sub演算
  ra[dst_reg] -= src_data;

#ifdef TRACE
  printf("trace: suba.%c   src=%d PC=%06lX\n", size_char[size], src_data,
         save_pc);
#endif

  return false;
}

/*
 　機能：subx命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Subx(char code1, char code2) {
  char size;
  Long dest_data;

  int src_reg = (code2 & 0x07);
  int dst_reg = ((code1 & 0x0E) >> 1);
  size = ((code2 >> 6) & 0x03);

  if ((code2 & 0x08) != 0) {
    /* -(An), -(An) */
    err68a("未定義命令を実行しました", __FILE__, __LINE__);
  }

#ifdef TEST_CCR
  short before = sr & 0x1f;
#endif
  dest_data = rd[dst_reg];

  bool save_z = CCR_Z_REF() != 0 ? true : false;
  rd[dst_reg] -= rd[src_reg] + (CCR_X_REF() ? 1 : 0);

  /* フラグの変化 */
  sub_conditions(rd[src_reg], dest_data, rd[dst_reg], size, save_z);

#ifdef TEST_CCR
  check("subx", rd[src_reg], dest_data, rd[dst_reg], size, before);
#endif

  return false;
}

/*
 　機能：sub Dn,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Sub1(char code1, char code2) {
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

#ifdef TEST_CCR
  short before = sr & 0x1f;
#endif

  /* Sub演算 */
  Long result = dest_data - src_data;

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

#ifdef TEST_CCR
  check("sub", src_data, dest_data, result, size, before);
#endif

  return false;
}

/*
 　機能：sub <ea>,Dn命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Sub2(char code1, char code2) {
  char size;
  char mode;
  char src_reg;
  Long src_data;
  Long dest_data;

  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  int dst_reg = ((code1 & 0x0E) >> 1);
  size = ((code2 >> 6) & 0x03);

  if (mode == EA_AD && size == S_BYTE) {
    err68a("不正な命令: sub.b An, Dn を実行しようとしました。", __FILE__,
           __LINE__);
  } else if (get_data_at_ea(EA_All, mode, src_reg, size, &src_data)) {
    return true;
  }

  /* レジスタへの格納である為、Long
   * で値を得ておかないと、格納時に上位ワードを破壊してしまう */
  if (get_data_at_ea(EA_All, EA_DD, dst_reg, S_LONG /*size*/, &dest_data)) {
    return true;
  }

#ifdef TEST_CCR
  short before = sr & 0x1f;
#endif

  SetDreg(dst_reg, dest_data - src_data, size);

  /* フラグの変化 */
  sub_conditions(src_data, dest_data, rd[dst_reg], size, true);

#ifdef TEST_CCR
  check("sub2", src_data, dest_data, rd[dst_reg], size, before);
#endif

  return false;
}

/*
 　機能：9ライン命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool line9(char *pc_ptr) {
  char code1, code2;

  code1 = *(pc_ptr++);
  code2 = *pc_ptr;
  pc += 2;

  if ((code2 & 0xC0) == 0xC0) {
    return (Suba(code1, code2));
  } else {
    if ((code1 & 0x01) == 1) {
      if ((code2 & 0x30) == 0x00)
        return (Subx(code1, code2));
      else
        return (Sub1(code1, code2));
    } else {
      return (Sub2(code1, code2));
    }
  }
}

/* $Id: line9.c,v 1.3 2009-08-08 06:49:44 masamic Exp $ */

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
 * Revision 1.5  1999/12/07  12:45:13  yfujii
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
