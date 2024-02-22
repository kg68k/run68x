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
 　機能：cmp命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Cmp(char code1, char code2) {
  char size;
  char mode;
  char src_reg;
  char dst_reg;
  Long src_data;
  Long dest_data;
#ifdef TRACE
  Long save_pc = pc;
#endif

  size = ((code2 >> 6) & 0x03);
  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  dst_reg = ((code1 & 0x0E) >> 1);

  /* ソースのアドレッシングモードに応じた処理 */
  if (mode == EA_AD && size == S_BYTE) {
    err68a("不正な命令: cmp.b An, Dn を実行しようとしました。", __FILE__,
           __LINE__);
  }
  if (get_data_at_ea(EA_All, mode, src_reg, size, &src_data)) {
    return true;
  }

  /* ディスティネーションのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_All, EA_DD, dst_reg, size, &dest_data)) {
    return true;
  }

#ifdef TEST_CCR
  short before = sr & 0x1f;
#endif

  Long result = dest_data - src_data;

  /* フラグの変化 */
  cmp_conditions(src_data, dest_data, result, size);

#ifdef TEST_CCR
  check("cmp", src_data, dest_data, result, size, before);
#endif

#ifdef TRACE
  switch (size) {
    case S_BYTE:
      rd[8] = (rd[dst_reg] & 0xFF);
      break;
    case S_WORD:
      rd[8] = (rd[dst_reg] & 0xFFFF);
      break;
    default: /* S_LONG */
      rd[8] = rd[dst_reg];
      break;
  }
  printf("trace: cmp.%c    src=%d dst=%d PC=%06lX\n", size_char[size], src_data,
         rd[8], save_pc);
#endif

  return false;
}

/*
 　機能：cmpa命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Cmpa(char code1, char code2) {
  char size;
  char mode;
  char src_reg;
  Long src_data;
  Long old;
  Long ans;
  Long dest_data;
#ifdef TEST_CCR
  short before;
#endif
#ifdef TRACE
  Long save_pc = pc;
#endif

  if ((code1 & 0x01) == 0)
    size = S_WORD;
  else
    size = S_LONG;
  mode = ((code2 & 0x38) >> 3);
  src_reg = (code2 & 0x07);
  int dst_reg = ((code1 & 0x0E) >> 1);

  /* ソースのアドレッシングモードに応じた処理 */
  if (size == S_BYTE) {
    err68a("不正な命令: cmp.b <ea>, An を実行しようとしました。", __FILE__,
           __LINE__);
  }
  if (get_data_at_ea(EA_All, mode, src_reg, size, &src_data)) {
    return true;
  }

  /* ディスティネーションのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_All, EA_AD, dst_reg, size, &dest_data)) {
    return true;
  }

  if (size == S_WORD) {
    if ((src_data & 0x8000) != 0) src_data |= 0xFFFF0000;
  }

#ifdef TRACE
  printf("trace: cmpa.%c   src=%d PC=%06lX\n", size_char[size], src_data,
         save_pc);
#endif

#ifdef TEST_CCR
  before = sr & 0x1f;
#endif
  old = ra[dst_reg];
  ans = old - src_data;

  /* フラグの変化 */
  cmp_conditions(src_data, old, ans, size);

#ifdef TEST_CCR
  check("cmpa", src_data, dest_data, ans, size, before);
#endif

  return false;
}

/*
 　機能：cmpm命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Cmpm(char code1, char code2) {
  char size;
  char src_reg;
  char dst_reg;
  Long src_data;
  Long dest_data;

  size = ((code2 >> 6) & 0x03);
  src_reg = (code2 & 0x07);
  dst_reg = ((code1 & 0x0E) >> 1);

  /* ソースのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_All, EA_AIPI, src_reg, size, &src_data)) {
    return true;
  }

  /* ディスティネーションのアドレッシングモードに応じた処理 */
  if (get_data_at_ea(EA_All, EA_AIPI, dst_reg, size, &dest_data)) {
    return true;
  }

  Long result = dest_data - src_data;

  /* フラグの変化 */
  cmp_conditions(src_data, dest_data, result, size);

#ifdef TRACE
  printf("trace: cmpm.%c   src=%d dst=%d PC=%06lX\n", size_char[size], src_data,
         rd[8], pc);
#endif

  return false;
}

/*
 　機能：eor命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Eor(char code1, char code2) {
  char size;
  char mode;
  char src_reg;
  char dst_reg;
  Long data;
  Long src_data;
  int work_mode;
#ifdef TRACE
  Long save_pc = pc;
#endif

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

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, dst_reg, size, &data)) {
    return true;
  }

  /* EOR演算 */
  data ^= src_data;

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_VariableData, work_mode, dst_reg, size, data)) {
    return true;
  }

  /* フラグの変化 */
  general_conditions(data, size);

#ifdef TRACE
  printf("trace: eor.%c    src=%d PC=%06lX\n", size_char[size], rd[src_reg],
         save_pc);
#endif

  return false;
}

/*
 　機能：Bライン命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool lineb(char *pc_ptr) {
  char code1, code2;

  code1 = *(pc_ptr++);
  code2 = *pc_ptr;
  pc += 2;

  if ((code1 & 0x01) == 0x00) {
    if ((code2 & 0xC0) == 0xC0) return (Cmpa(code1, code2));
    return (Cmp(code1, code2));
  }

  if ((code2 & 0xC0) == 0xC0) return (Cmpa(code1, code2));

  if ((code2 & 0x38) == 0x08) return (Cmpm(code1, code2));

  return (Eor(code1, code2));
}

/* $Id: lineb.c,v 1.2 2009/08/08 06:49:44 masamic Exp $ */

/*
 * $Log: lineb.c,v $
 * Revision 1.2  2009/08/08 06:49:44  masamic
 * Convert Character Encoding Shifted-JIS to UTF-8.
 *
 * Revision 1.1.1.1  2001/05/23 11:22:08  masamic
 * First imported source code and docs
 *
 * Revision 1.7  1999/12/21  10:08:59  yfujii
 * Uptodate source code from Beppu.
 *
 * Revision 1.6  1999/12/07  12:45:26  yfujii
 * *** empty log message ***
 *
 * Revision 1.6  1999/11/22  03:57:08  yfujii
 * Condition code calculations are rewriten.
 *
 * Revision 1.4  1999/10/25  04:22:27  masamichi
 * Full implements EOR instruction.
 *
 * Revision 1.3  1999/10/20  04:14:48  masamichi
 * Added showing more information about errors.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
