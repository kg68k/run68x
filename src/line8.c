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
  UShort waru;
  ULong data;
  ULong ans;
  UShort mod;
  Long waru_l;

  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  int dst_reg = (code1 & 0x0E) >> 1;
  data = rd[dst_reg];

  /* ソースのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_Data, mode, src_reg, S_WORD, &waru_l)) {
    return true;
  }
  waru = (UShort)waru_l;

  if (waru == 0) {
    err68a("０で除算しました", __FILE__, __LINE__);
  }

  CCR_C_OFF();
  ans = data / waru;
  mod = (unsigned char)(data % waru);
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
  short waru;
  Long data;
  Long ans;
  short mod;
  Long waru_l;

  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  int dst_reg = ((code1 & 0x0E) >> 1);
  data = rd[dst_reg];

  /* ソースのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_Data, mode, src_reg, S_WORD, &waru_l)) {
    return true;
  }

  waru = (UShort)waru_l;

  if (waru == 0) {
    err68a("０で除算しました", __FILE__, __LINE__);
  }

  CCR_C_OFF();
  ans = data / waru;
  mod = data % waru;
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
  if (((code1 & 0x01) == 0x01) && ((code2 & 0xF0) == 0)) {
    /* sbcd */
    char src_reg = (code2 & 0x7);
    char dst_reg = ((code1 & 0xE) >> 1);
    char size = 0; /* S_BYTE 固定 */
    Long src_data;
    Long dst_data;
    Long kekka;
    Long X;

    if ((code2 & 0x8) != 0) {
      /* -(am),-(an); */
      if (get_data_at_ea(EA_All, EA_AIPD, src_reg, size, &src_data)) {
        return true;
      }
      if (get_data_at_ea(EA_All, EA_AIPD, dst_reg, size, &dst_data)) {
        return true;
      }
    } else {
      /* dm,dn; */
      if (get_data_at_ea(EA_All, EA_DD, src_reg, size, &src_data)) {
        return true;
      }
      if (get_data_at_ea(EA_All, EA_DD, dst_reg, size, &dst_data)) {
        return true;
      }
    }

    X = (CCR_X_REF() != 0) ? 1 : 0;

    kekka = dst_data - src_data - X;

    if ((dst_data & 0xff) < ((src_data & 0xff) + X)) kekka -= 0x60;

    if ((dst_data & 0x0f) < ((src_data & 0x0f) + X)) kekka -= 0x06;

    if ((dst_data ^ kekka) & 0x100) {
      CCR_X_ON();
      CCR_C_ON();
    } else {
      CCR_X_OFF();
      CCR_C_OFF();
    }

    kekka &= 0xff;

    /* 0 以外の値になった時のみ、Z フラグをリセットする */
    if (kekka != 0) {
      CCR_Z_OFF();
    }

    /* Nフラグは結果に応じて立てる */
    if (kekka & 0x80) {
      CCR_N_ON();
    } else {
      CCR_N_OFF();
    }

    /* Vフラグ */
    if ((dst_data <= kekka) && (0x20 <= kekka) && (kekka < 0x80)) {
      CCR_V_ON();
    } else {
      CCR_V_OFF();
    }

    dst_data = kekka;

    if ((code2 & 0x8) != 0) {
      /* -(am),-(an); */
      if (set_data_at_ea(EA_All, EA_AI, dst_reg, size, dst_data)) {
        return true;
      }
    } else {
      /* dm,dn; */
      if (set_data_at_ea(EA_All, EA_DD, dst_reg, size, dst_data)) {
        return true;
      }
    }

    return false;
  }

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
