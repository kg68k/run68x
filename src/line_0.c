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
 　機能：ori命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Ori(char code) {
  Long src_data;
  char mode;
  char reg;
  int work_mode;
  Long data;

  char size = ((code >> 6) & 0x03);
  if (size == 3) return IllegalInstruction();

  mode = (code & 0x38) >> 3;
  reg = (code & 0x07);
  src_data = imi_get(size);

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &data)) {
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

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, data)) {
    return true;
  }

  /* フラグのセット */
  general_conditions(data, size);

  return false;
}

/*
 　機能：ori to CCR命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Ori_t_ccr() {
  UByte data = imi_get(S_BYTE);

#ifdef TRACE
  printf("trace: ori_t_ccr src=0x%02X PC=%06lX\n", data, pc - 2);
#endif
  sr |= (data & CCR_MASK);
  return false;
}

/*
 　機能：ori to SR命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Ori_t_sr() {
  short data;

  if (SR_S_REF() == 0) {
    err68a("特権命令を実行しました", __FILE__, __LINE__);
  }

  data = (short)imi_get(S_WORD);

#ifdef TRACE
  printf("trace: ori_t_sr src=0x%02X PC=%06lX\n", data, pc - 2);
#endif

  /* SRをセット */
  sr |= data;

  return false;
}

/*
 　機能：andi命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Andi(char code) {
  Long src_data;
  char mode;
  char reg;
  Long work_mode;
  Long data;

  char size = ((code >> 6) & 0x03);
  if (size == 3) IllegalInstruction();

  mode = (code & 0x38) >> 3;
  reg = (code & 0x07);

  src_data = imi_get(size);

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &data)) {
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

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, data)) {
    return true;
  }

  /* フラグのセット */
  general_conditions(data, size);

  return false;
}

/*
 　機能：andi to CCR命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Andi_t_ccr() {
  UByte data = (char)imi_get(S_BYTE);

#ifdef TRACE
  printf("trace: andi_t_ccr src=0x%02X PC=%06lX\n", data, pc - 2);
#endif
  sr &= (data | ~CCR_MASK);
  return false;
}

/*
 　機能：andi to SR命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Andi_t_sr() {
  short data;

  if (SR_S_REF() == 0) {
    err68a("特権命令を実行しました", __FILE__, __LINE__);
  }

  data = (short)imi_get(S_WORD);

#ifdef TRACE
  printf("trace: andi_t_sr src=0x%02X PC=%06lX\n", data, pc - 2);
#endif

  /* SRをセット */
  sr &= data;

  return false;
}

/*
 　機能：addi命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Addi(char code) {
  Long src_data;
  char mode;
  char reg;
  int work_mode;
  Long dest_data;

  char size = ((code >> 6) & 0x03);
  if (size == 3) return IllegalInstruction();

  mode = (code & 0x38) >> 3;
  reg = (code & 0x07);

  src_data = imi_get(size);

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &dest_data)) {
    return true;
  }

#ifdef TEST_CCR
  short before = sr & 0x1f;
#endif

  /* Add演算 */
  Long result = dest_data + src_data;

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, result)) {
    return true;
  }

  /* フラグの変化 */
  add_conditions(src_data, dest_data, result, size, true);

#ifdef TEST_CCR
  check("addi", src_data, dest_data, result, size, before);
#endif

  return false;
}

/*
 　機能：subi命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Subi(char code) {
  Long src_data;
  char mode;
  char reg;
  int work_mode;
  Long dest_data;

  char size = ((code >> 6) & 0x03);
  if (size == 3) return IllegalInstruction();

  mode = (code & 0x38) >> 3;
  reg = (code & 0x07);

  src_data = imi_get(size);

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &dest_data)) {
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

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, result)) {
    return true;
  }

  /* フラグの変化 */
  sub_conditions(src_data, dest_data, result, size, true);

#ifdef TEST_CCR
  check("subi", src_data, dest_data, result, size, before);
#endif

  return false;
}

/*
 　機能：eori命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Eori(char code) {
  char mode;
  char reg;
  Long data;
  Long src_data;
  Long work_mode;

  char size = ((code >> 6) & 0x03);
  if (size == 3) return IllegalInstruction();

  mode = ((code & 0x38) >> 3);
  reg = (code & 0x07);

  src_data = imi_get(size);

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &data)) {
    return true;
  }

  /* Eor演算 */
  data ^= src_data;

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, data)) {
    return true;
  }

  /* フラグのセット */
  general_conditions(data, size);

  return false;
}

/*
 　機能：eori to CCR命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Eori_t_ccr() {
  UByte data = imi_get(S_BYTE);

#ifdef TRACE
  printf("trace: eori_t_ccr src=0x%02X PC=%06lX\n", data, pc - 2);
#endif
  sr ^= (data & CCR_MASK);
  return false;
}

/*
 　機能：cmpi命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Cmpi(char code) {
  char mode;
  char reg;
  Long src_data;
  short save_x;
  Long dest_data;

  char size = ((code >> 6) & 0x03);
  if (size == 3) return IllegalInstruction();

  mode = (code & 0x38) >> 3;
  reg = (code & 0x07);
  save_x = CCR_X_REF();

  src_data = imi_get(size);

  if (get_data_at_ea(EA_VariableData, mode, reg, size, &dest_data)) {
    return true;
  }

#ifdef TEST_CCR
  short before = sr & 0x1f;
#endif

  /* Sub演算 */
  Long result = dest_data - src_data;

  if (save_x == 0)
    CCR_X_OFF();
  else
    CCR_X_ON();

  /* フラグの変化 */
  cmp_conditions(src_data, dest_data, result, size);

#ifdef TEST_CCR
  check("cmpi", src_data, dest_data, result, size, before);
#endif

  return false;
}

/*
 　機能：btst #data,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Btsti(char code) {
  char mode;
  char reg;
  Long data;
  Long mask = 1;
  int size;
#ifdef TRACE
  Long save_pc = pc;
#endif

  mode = (code & 0x38) >> 3;
  reg = (code & 0x07);
  unsigned int bitno = imi_get(S_BYTE);
  if (mode == MD_DD) {
    bitno = (bitno % 32);
    size = S_LONG;
  } else {
    bitno = (bitno % 8);
    size = S_BYTE;
  }

  mask <<= bitno;

  /* 実効アドレスで示されたデータを取得 */
  if (get_data_at_ea(EA_Data, mode, reg, size, &data)) {
    return true;
  }

  /* Zフラグに反映 */
  if ((data & mask) == 0)
    CCR_Z_ON();
  else
    CCR_Z_OFF();

#ifdef TRACE
  printf("trace: btst     src=%d PC=%06lX\n", bitno, save_pc);
#endif

  return false;
}

/*
 　機能：btst Dn,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Btst(char code1, char code2) {
  char mode;
  char reg;
  Long data;
  Long mask = 1;
  int size;
#ifdef TRACE
  Long save_pc = pc;
#endif

  mode = (code2 & 0x38) >> 3;
  reg = (code2 & 0x07);

  unsigned int bitno = rd[(code1 >> 1) & 0x07];
  if (mode == MD_DD) {
    bitno = (bitno % 32);
    size = S_LONG;
  } else {
    bitno = (bitno % 8);
    size = S_BYTE;
  }

  mask <<= bitno;

  /* 実効アドレスで示されたデータを取得 */
  if (get_data_at_ea(EA_Data, mode, reg, size, &data)) {
    return true;
  }

  /* Zフラグに反映 */
  if ((data & mask) == 0)
    CCR_Z_ON();
  else
    CCR_Z_OFF();

#ifdef TRACE
  printf("trace: btst     src=%d PC=%06lX\n", bitno, save_pc);
#endif

  return false;
}

/*
 　機能：bchg #data,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Bchgi(char code) {
  char mode;
  char reg;
  Long mask = 1;
  int size;
  int work_mode;
  Long data;

  mode = (code & 0x38) >> 3;
  reg = (code & 0x07);
  unsigned int bitno = imi_get(S_BYTE);

  if (mode == MD_DD) {
    bitno = (bitno % 32);
    size = S_LONG;
  } else {
    bitno = (bitno % 8);
    size = S_BYTE;
  }

  mask <<= bitno;

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &data)) {
    return true;
  }

  /* bchg演算 */
  if ((data & mask) == 0) {
    CCR_Z_ON();
    data |= mask;
  } else {
    CCR_Z_OFF();
    data &= ~mask;
  }

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, data)) {
    return true;
  }

  return false;
}

/*
 　機能：bchg Dn,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Bchg(char code1, char code2) {
  char mode;
  char reg;
  Long data;
  Long mask = 1;
  int size;
  int work_mode;

  mode = (code2 & 0x38) >> 3;
  reg = (code2 & 0x07);

  unsigned int bitno = rd[(code1 >> 1) & 0x07];
  if (mode == MD_DD) {
    bitno = (bitno % 32);
    size = S_LONG;
  } else {
    bitno = (bitno % 8);
    size = S_BYTE;
  }

  mask <<= bitno;

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &data)) {
    return true;
  }

  /* bchg演算 */
  if ((data & mask) == 0) {
    CCR_Z_ON();
    data |= mask;
  } else {
    CCR_Z_OFF();
    data &= ~mask;
  }

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, data)) {
    return true;
  }

  return false;
}

/*
 　機能：bclr #data,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Bclri(char code) {
  char mode;
  char reg;
  Long data;
  Long mask = 1;
  int size;
  int work_mode;

  mode = (code & 0x38) >> 3;
  reg = (code & 0x07);

  unsigned int bitno = imi_get(S_BYTE);
  if (mode == MD_DD) {
    bitno = (bitno % 32);
    size = S_LONG;
  } else {
    bitno = (bitno % 8);
    size = S_BYTE;
  }

  mask <<= bitno;

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &data)) {
    return true;
  }

  /* bclr演算 */
  if ((data & mask) == 0) {
    CCR_Z_ON();
  } else {
    CCR_Z_OFF();
    data &= ~mask;
  }

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, data)) {
    return true;
  }

  return false;
}

/*
 　機能：bclr Dn,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Bclr(char code1, char code2) {
  char mode;
  char reg;
  Long data;
  Long mask = 1;
  int size;
  int work_mode;

  mode = (code2 & 0x38) >> 3;
  reg = (code2 & 0x07);

  unsigned int bitno = rd[(code1 >> 1) & 0x07];
  if (mode == MD_DD) {
    bitno = (bitno % 32);
    size = S_LONG;
  } else {
    bitno = (bitno % 8);
    size = S_BYTE;
  }

  mask <<= bitno;

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &data)) {
    return true;
  }

  /* bclr演算 */
  if ((data & mask) == 0) {
    CCR_Z_ON();
  } else {
    CCR_Z_OFF();
    data &= ~mask;
  }

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, data)) {
    return true;
  }

  return false;
}

/*
 　機能：bset #data,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Bseti(char code) {
  char mode;
  char reg;
  Long data;
  ULong mask = 1;
  int size;
  int work_mode;

  mode = (code & 0x38) >> 3;
  reg = (code & 0x07);

  unsigned int bitno = imi_get(S_BYTE);
  if (mode == MD_DD) {
    bitno = (bitno % 32);
    size = S_LONG;
  } else {
    bitno = (bitno % 8);
    size = S_BYTE;
  }

  mask <<= bitno;

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &data)) {
    return true;
  }

  /* bset演算 */
  if ((data & mask) == 0) {
    CCR_Z_ON();
    data |= mask;
  } else {
    CCR_Z_OFF();
  }

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, data)) {
    return true;
  }

  return false;
}

/*
 　機能：bset Dn,<ea>命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Bset(char code1, char code2) {
  char mode;
  char reg;
  Long data;
  ULong mask = 1;
  int size;
  int work_mode;

  mode = (code2 & 0x38) >> 3;
  reg = (code2 & 0x07);

  unsigned int bitno = rd[(code1 >> 1) & 0x07];
  if (mode == MD_DD) {
    bitno = (bitno % 32);
    size = S_LONG;
  } else {
    bitno = (bitno % 8);
    size = S_BYTE;
  }

  mask <<= bitno;

  /* アドレッシングモードがポストインクリメント間接の場合は間接でデータの取得 */
  if (mode == EA_AIPI) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (get_data_at_ea_noinc(EA_VariableData, work_mode, reg, size, &data)) {
    return true;
  }

  /* bset演算 */
  if ((data & mask) == 0) {
    CCR_Z_ON();
    data |= mask;
  } else {
    CCR_Z_OFF();
  }

  /* アドレッシングモードがプレデクリメント間接の場合は間接でデータの設定 */
  if (mode == EA_AIPD) {
    work_mode = EA_AI;
  } else {
    work_mode = mode;
  }

  if (set_data_at_ea(EA_VariableData, work_mode, reg, size, data)) {
    return true;
  }

  return false;
}

/*
 　機能：movep from Dn命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Movep_f(char code1, char code2) {
  int d_reg;
  int a_reg;

  d_reg = ((code1 >> 1) & 0x07);
  a_reg = (code2 & 0x07);
  Long adr = ra[a_reg] + extl(imi_get_word());

  if ((code2 & 0x40) != 0) {
    /* LONG */
    mem_set(adr, ((rd[d_reg] >> 24) & 0xFF), S_BYTE);
    mem_set(adr + 2, ((rd[d_reg] >> 16) & 0xFF), S_BYTE);
    mem_set(adr + 4, ((rd[d_reg] >> 8) & 0xFF), S_BYTE);
    mem_set(adr + 6, rd[d_reg] & 0xFF, S_BYTE);
  } else {
    /* WORD */
    mem_set(adr, ((rd[d_reg] >> 8) & 0xFF), S_BYTE);
    mem_set(adr + 2, rd[d_reg] & 0xFF, S_BYTE);
  }

#ifdef TRACE
  printf("trace: movep_f  src=%d PC=%06lX\n", rd[d_reg], pc - 2);
#endif

  return false;
}

/*
 　機能：movep to Dn命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
static bool Movep_t(char code1, char code2) {
  int d_reg;
  int a_reg;
  ULong data;

  d_reg = ((code1 >> 1) & 0x07);
  a_reg = (code2 & 0x07);
  Long adr = ra[a_reg] + extl(imi_get_word());

  data = mem_get(adr, S_BYTE);
  data = ((data << 8) | (mem_get(adr + 2, S_BYTE) & 0xFF));
  if ((code2 & 0x40) != 0) { /* LONG */
    data = ((data << 8) | (mem_get(adr + 4, S_BYTE) & 0xFF));
    data = ((data << 8) | (mem_get(adr + 6, S_BYTE) & 0xFF));
    rd[d_reg] = data;
  } else {
    rd[d_reg] = ((rd[d_reg] & 0xFFFF0000) | (data & 0xFFFF));
  }

#ifdef TRACE
  printf("trace: movep_t  PC=%06lX\n", pc - 2);
#endif

  return false;
}

/*
 　機能：0ライン命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool line0(char *pc_ptr) {
  char code1, code2;

  code1 = *(pc_ptr++);
  code2 = *pc_ptr;
  pc += 2;

  switch (code1) {
    case 0x00:
      if (code2 == 0x3C)
        return (Ori_t_ccr());
      else if (code2 == 0x7C)
        return (Ori_t_sr());
      else
        return (Ori(code2));
    case 0x02:
      if (code2 == 0x3C)
        return (Andi_t_ccr());
      else if (code2 == 0x7C)
        return (Andi_t_sr());
      else
        return (Andi(code2));
    case 0x04:
      return (Subi(code2));
    case 0x06:
      return (Addi(code2));
    case 0x08:
      switch (code2 & 0xC0) {
        case 0x00:
          return (Btsti(code2));
        case 0x40:
          return (Bchgi(code2));
        case 0x80:
          return (Bclri(code2));
        default: /* 0xC0 */
          return (Bseti(code2));
      }
    case 0x0A:
      if (code2 == 0x3C) return (Eori_t_ccr());
      if (code2 == 0x7C) { /* eori to SR */
        break;
      }
      return (Eori(code2));
    case 0x0C:
      return (Cmpi(code2));
    default:
      if ((code2 & 0x38) == 0x08) {
        if ((code2 & 0x80) != 0)
          return (Movep_f(code1, code2));
        else
          return (Movep_t(code1, code2));
      }
      switch (code2 & 0xC0) {
        case 0x00:
          return (Btst(code1, code2));
        case 0x40:
          return (Bchg(code1, code2));
        case 0x80:
          return (Bclr(code1, code2));
        default: /* 0xC0 */
          return (Bset(code1, code2));
      }
  }

  return IllegalInstruction();
}

/* $Id: line0.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:07  masamic
 * First imported source code and docs
 *
 * Revision 1.6  1999/12/21  10:08:59  yfujii
 * Uptodate source code from Beppu.
 *
 * Revision 1.5  1999/12/07  12:43:24  yfujii
 * *** empty log message ***
 *
 * Revision 1.5  1999/11/22  03:57:08  yfujii
 * Condition code calculations are rewriten.
 *
 * Revision 1.3  1999/10/20  02:39:39  masamichi
 * Add showing more information about errors.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
