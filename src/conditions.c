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

#include "run68.h"

static void ccr2bitmap(UWord ccr, char *bitmap) {
  int i;
  int j = 0;

  ccr &= CCR_MASK;

  for (i = 6; i >= 0; i--) {
    int flag = (ccr >> i) & 1;
    if (flag == 1) {
      bitmap[j++] = '1';
    } else {
      bitmap[j++] = '0';
    }
  }
  bitmap[j] = '\0';
}

void check(char *mode, Long src, Long dest, Long result, int size,
           short before) {
  char befstr[9];
  char aftstr[9];

  ccr2bitmap(before, befstr);
  ccr2bitmap(sr, aftstr);

  printf("%s: 0x%08x 0x%08x 0x%08x %1d %8s %8s\n", mode, src, dest, result,
         size, befstr, aftstr);
}

Long getMSB(Long num, int size) {
  switch (size) {
    case S_BYTE:
      return (num >> 7) & 1;
    case S_WORD:
      return (num >> 15) & 1;
    case S_LONG:
      return (num >> 31) & 1;

    default:
      err68a("不正なデータサイズです。", __FILE__, __LINE__);
  }
}

Long getBitsByDataSize(Long num, int size) {
  switch (size) {
    case S_BYTE:
      return num & 0xff;
    case S_WORD:
      return num & 0xffff;
    case S_LONG:
      return num;

    default:
      err68a("不正なデータサイズです。", __FILE__, __LINE__);
  }
}

/*
 * 【説明】
 *   一般系コンディションフラグの設定
 *
 * 【レジスタの変化】
 *   X: 変化なし
 *   N: 負数のときON、零または正数のときOFF
 *   Z: 零のときON、零以外のときOFF
 *   V: 常に0
 *   C: 常に0
 *
 * 【関数書式】
 *   general_conditions(result, size);
 *
 * 【引数】
 *   Long result;    <in>  Result値
 *   int  size;      <in>  アクセスサイズ
 *
 * 【返値】
 *   なし
 *
 */

void general_conditions(Long result, int size) {
  int Rm;

  Rm = (getMSB(result, size) != (Long)0);

  /* Overflow Flag */
  CCR_V_OFF();

  /* Carry Flag & Extend Flag */
  CCR_C_OFF();
  //	CCR_X_OFF();

  /* Zero Flag */
  if (getBitsByDataSize(result, size) == 0) {
    CCR_Z_ON();
  } else {
    CCR_Z_OFF();
  }

  /* Negative Flag */
  if (Rm != 0) {
    CCR_N_ON();
  } else {
    CCR_N_OFF();
  }
}

/*
 * 【説明】
 *   add系コンディションフラグの設定
 *
 * 【関数書式】
 *   add_conditions(src, dest, result, size, zero_flag);
 *
 * 【引数】
 *   Long src;       <in>  Source値
 *   Long dest;      <in>  Destination値
 *   Long result;    <in>  Result値
 *   int  size;      <in>  アクセスサイズ
 *   bool zero_flag; <in>  addx用演算前 zero flag 値。
 *                         その他の場合は常に true を指定のこと。
 *
 * 【返値】
 *   なし
 *
 */

void add_conditions(Long src, Long dest, Long result, int size,
                    bool zero_flag) {
  int Sm, Dm, Rm;

  Sm = (getMSB(src, size) != (Long)0);
  Dm = (getMSB(dest, size) != (Long)0);
  Rm = (getMSB(result, size) != (Long)0);

  /* Overflow Flag */
  if ((Sm && Dm && !Rm) || (!Sm && !Dm && Rm)) {
    CCR_V_ON();
  } else {
    CCR_V_OFF();
  }

  /* Carry Flag & Extend Flag */
  if ((Sm && Dm) || (Dm && !Rm) || (Sm && !Rm)) {
    CCR_X_C_ON();
  } else {
    CCR_X_C_OFF();
  }

  /* Zero Flag */
  if (zero_flag && getBitsByDataSize(result, size) == 0) {
    CCR_Z_ON();
  } else {
    CCR_Z_OFF();
  }

  /* Negative Flag */
  if (Rm != 0) {
    CCR_N_ON();
  } else {
    CCR_N_OFF();
  }
}

/*
 * 【説明】
 *   cmp系コンディションフラグの設定
 *
 * 【関数書式】
 *   cmp_conditions(src, dest, result, size);
 *
 * 【引数】
 *   Long src;       <in>  Source値
 *   Long dest;      <in>  Destination値
 *   Long result;    <in>  Result値
 *   int  size;      <in>  アクセスサイズ
 *
 * 【返値】
 *   なし
 *
 */

void cmp_conditions(Long src, Long dest, Long result, int size) {
  int Sm, Dm, Rm;

  Sm = (getMSB(src, size) != (Long)0);
  Dm = (getMSB(dest, size) != (Long)0);
  Rm = (getMSB(result, size) != (Long)0);

  /* Overflow Flag */
  if ((!Sm && Dm && !Rm) || (Sm && !Dm && Rm)) {
    CCR_V_ON();
  } else {
    CCR_V_OFF();
  }

  /* Carry Flag & Extend Flag */
  if ((Sm && !Dm) || (!Dm && Rm) || (Sm && Rm)) {
    CCR_C_ON();
  } else {
    CCR_C_OFF();
  }

  /* Zero Flag */
  if (getBitsByDataSize(result, size) == 0) {
    CCR_Z_ON();
  } else {
    CCR_Z_OFF();
  }

  /* Negative Flag */
  if (Rm != 0) {
    CCR_N_ON();
  } else {
    CCR_N_OFF();
  }
}

/*
 * 【説明】
 *   sub系コンディションフラグの設定
 *
 * 【関数書式】
 *   sub_conditions(src, dest, result, size, zero_flag);
 *
 * 【引数】
 *   Long src;       <in>  Source値
 *   Long dest;      <in>  Destination値
 *   Long result;    <in>  Result値
 *   int  size;      <in>  アクセスサイズ
 *   bool zero_flag; <in>  subx用演算前 zero flag 値。
 *                         その他の場合は常に bool を指定のこと。
 *
 * 【返値】
 *   なし
 *
 */

void sub_conditions(Long src, Long dest, Long result, int size,
                    bool zero_flag) {
  cmp_conditions(src, dest, result, size);

  if (CCR_C_REF()) {
    CCR_X_ON();
  } else {
    CCR_X_OFF();
  }

  /* Zero Flag */
  if (zero_flag && (CCR_Z_REF() != 0)) {
    CCR_Z_ON();
  } else {
    CCR_Z_OFF();
  }
}

/*
 * 【説明】
 *   neg系コンディションフラグの設定
 *
 * 【関数書式】
 *   neg_conditions(dest, result, size, zero_flag);
 *
 * 【引数】
 *   Long dest;      <in>  Destination値
 *   Long result;    <in>  Result値
 *   int  size;      <in>  アクセスサイズ
 *   bool zero_flag; <in>  negx用演算前 zero flag 値。
 *                         その他の場合は常に true を指定のこと。
 *
 * 【返値】
 *   なし
 *
 */

void neg_conditions(Long dest, Long result, int size, bool zero_flag) {
  int Dm, Rm;

  Dm = (getMSB(dest, size) != (Long)0);
  Rm = (getMSB(result, size) != (Long)0);

  /* Overflow Flag */
  if (Dm && Rm) {
    CCR_V_ON();
  } else {
    CCR_V_OFF();
  }

  /* Carry Flag & Extend Flag */
  if (Dm || Rm) {
    CCR_X_C_ON();
  } else {
    CCR_X_C_OFF();
  }

  /* Zero Flag */
  if (zero_flag && getBitsByDataSize(result, size) == 0) {
    CCR_Z_ON();
  } else {
    CCR_Z_OFF();
  }

  /* Negative Flag */
  if (Rm != 0) {
    CCR_N_ON();
  } else {
    CCR_N_OFF();
  }
}

/* $Id: conditions.c,v 1.3 2009-08-08 06:49:44 masamic Exp $ */

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
 * Revision 1.1.1.1  2001/05/23 11:22:05  masamic
 * First imported source code and docs
 *
 * Revision 1.2  1999/12/07  12:39:54  yfujii
 * *** empty log message ***
 *
 * Revision 1.2  1999/11/29  06:24:04  yfujii
 * Condition code operations are modified to be correct.
 *
 *
 */
