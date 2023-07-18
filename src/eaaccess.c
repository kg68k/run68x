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

/* Get from / Set to Effective Address */

#include <stdbool.h>

#include "mem.h"
#include "run68.h"

/*
 * 【説明】
 *   実効アドレスを取得する。
 *
 * 【関数書式】
 *   retcode = get_ea(save_pc, AceptAdrMode, mode, reg, &data);
 *
 * 【引数】
 *   Long save_pc;      <in>  PC相対時の基準となるPC値
 *   int  AceptAdrMode; <in>  アドレッシングモード MD_??
 *   int  mode;         <in>  アドレッシングモード MD_??
 *   int  reg;          <in>  レジスタ番号またはアドレッシングモード　MR_??
 *   Long *data;        <out> 取得するデータを格納する場所へのポインタ
 *
 * 【返値】
 *   true:  エラー
 *   false: 正常
 *
 */

bool get_ea(Long save_pc, int AceptAdrMode, int mode, int reg, Long *data) {
  /* 操作しやすいようにモードを統合 */
  int gmode = (mode < 7) ? mode : (7 + reg); /* gmode = 0-11 */

  /* AceptAdrMode で許されたアドレッシングモードでなければエラー */
  if ((AceptAdrMode & (1 << gmode)) == 0) {
    err68a("アドレッシングモードが異常です。", __FILE__, __LINE__);
  }

  /* アドレッシングモードに応じた処理 */
  switch (gmode) {
    case EA_AI:
      *data = ra[reg];
      break;
    case EA_AID:
      *data = ra[reg] + extl(imi_get_word());
      break;
    case EA_AIX:
      *data = ra[reg] + idx_get();
      break;
    case EA_SRT:
      *data = extl(imi_get_word());
      break;
    case EA_LNG:
      *data = imi_get(S_LONG);
      break;
    case EA_PC:
      *data = save_pc + extl(imi_get_word());
      break;
    case EA_PCX:
      *data = save_pc + idx_get();
      break;
    default:
      err68a("アドレッシングモードが異常です。", __FILE__, __LINE__);
  }
  return false;
}

/* Get Data at Effective Address */

/*
 * 【説明】
 *   実効アドレスで示された値を取得する。
 *
 * 【関数書式】
 *   retcode = get_data_at_ea(AceptAdrMode, mode, reg, &data);
 *
 * 【引数】
 *   int AceptAdrMode; <in>  処理可能なアドレッシングモード群 EA_????*
 *   int mode;         <in>  アドレッシングモード MD_??
 *   int reg;          <in>  レジスタ番号またはアドレッシングモード　MR_??
 *   Long *data;       <out> 取得するデータを格納する場所へのポインタ
 *
 * 【返値】
 *   true:  エラー
 *   false: 正常
 *
 */

bool get_data_at_ea(int AceptAdrMode, int mode, int reg, int size, Long *data) {
  Long save_pc = pc;

  /* 操作しやすいようにモードを統合 */
  int gmode = mode < 7 ? mode : 7 + reg; /* gmode = 0-11 */

  /* AceptAdrMode で許されたアドレッシングモードでなければエラー */
  if ((AceptAdrMode & (1 << gmode)) == 0) {
    err68a("アドレッシングモードが異常です。", __FILE__, __LINE__);
  }

  /* アドレッシングモードに応じた処理 */
  switch (gmode) {
    case EA_DD:
      switch (size) {
        case S_BYTE:
          *data = (rd[reg] & 0xFF);
          break;
        case S_WORD:
          *data = (rd[reg] & 0xFFFF);
          break;
        case S_LONG:
          *data = rd[reg];
          break;
      }
      break;
    case EA_AD:
      switch (size) {
        case S_BYTE:
          *data = (ra[reg] & 0xFF);
          break;
        case S_WORD:
          *data = (ra[reg] & 0xFFFF);
          break;
        case S_LONG:
          *data = ra[reg];
          break;
      }
      break;
    case EA_AI:
      *data = mem_get(ra[reg], (char)size);
      break;
    case EA_AIPI:
      *data = mem_get(ra[reg], (char)size);
      if (reg == 7 && size == S_BYTE) {
        /* システムスタックのポインタは常に偶数 */
        inc_ra(reg, (char)S_WORD);
      } else {
        inc_ra(reg, (char)size);
      }
      break;
    case EA_AIPD:
      if (reg == 7 && size == S_BYTE) {
        /* システムスタックのポインタは常に偶数 */
        dec_ra(reg, (char)S_WORD);
      } else {
        dec_ra(reg, (char)size);
      }
      *data = mem_get(ra[reg], (char)size);
      break;
    case EA_AID:
      *data = mem_get(ra[reg] + extl(imi_get_word()), (char)size);
      break;
    case EA_AIX:
      *data = mem_get(ra[reg] + idx_get(), (char)size);
      break;
    case EA_SRT:
      *data = mem_get(extl(imi_get_word()), (char)size);
      break;
    case EA_LNG:
      *data = mem_get(imi_get(S_LONG), (char)size);
      break;
    case EA_PC:
      *data = mem_get(save_pc + extl(imi_get_word()), (char)size);
      break;
    case EA_PCX:
      *data = mem_get(save_pc + idx_get(), (char)size);
      break;
    case EA_IM:
      *data = imi_get((char)size);
      break;
    default:
      err68a("アドレッシングモードが異常です。", __FILE__, __LINE__);
  }

  return false;
}

/*
 * 【説明】
 *   与えられたデータを実効アドレスで示された場所に設定する。
 *
 * 【関数書式】
 *   retcode = set_data_at_ea(AceptAdrMode, mode, reg, data);
 *
 * 【引数】
 *   int AceptAdrMode; <in>  処理可能なアドレッシングモード群 EA_????*
 *   int mode;         <in>  アドレッシングモード MD_??
 *   int reg;          <in>  レジスタ番号またはアドレッシングモード　MR_??
 *   Long data;        <in>  設定するデータ
 *
 * 【返値】
 *   true:  エラー
 *   false: 正常
 *
 */

bool set_data_at_ea(int AceptAdrMode, int mode, int reg, int size, Long data) {
  Long save_pc = pc;

  /* 操作しやすいようにモードを統合 */
  int gmode = mode < 7 ? mode : 7 + reg; /* gmode = 0-11 */

  /* AceptAdrMode で許されたアドレッシングモードでなければエラー */

  if ((AceptAdrMode & (1 << gmode)) == 0) {
    err68a("アドレッシングモードが異常です。", __FILE__, __LINE__);
  }

  /* ディスティネーションのアドレッシングモードに応じた処理 */
  switch (gmode) {
    case EA_DD:
      switch (size) {
        case S_BYTE:
          rd[reg] = (rd[reg] & 0xFFFFFF00) | (data & 0xFF);
          break;
        case S_WORD:
          rd[reg] = (rd[reg] & 0xFFFF0000) | (data & 0xFFFF);
          break;
        case S_LONG:
          rd[reg] = data;
          break;
      }
      break;
    case EA_AD:
      switch (size) {
        case S_BYTE:
          ra[reg] = (ra[reg] & 0xFFFFFF00) | (data & 0xFF);
          break;
        case S_WORD:
          ra[reg] = (ra[reg] & 0xFFFF0000) | (data & 0xFFFF);
          break;
        case S_LONG:
          ra[reg] = data;
          break;
      }
      break;
    case EA_AI:
      mem_set(ra[reg], data, (char)size);
      break;
    case EA_AIPI:
      mem_set(ra[reg], data, (char)size);
      if (reg == 7 && size == S_BYTE) {
        /* システムスタックのポインタは常に偶数 */
        inc_ra(reg, (char)S_WORD);
      } else {
        inc_ra(reg, (char)size);
      }
      break;
    case EA_AIPD:
      if (reg == 7 && size == S_BYTE) {
        /* システムスタックのポインタは常に偶数 */
        dec_ra(reg, (char)S_WORD);
      } else {
        dec_ra(reg, (char)size);
      }

      mem_set(ra[reg], data, (char)size);
      break;
    case EA_AID:
      mem_set(ra[reg] + extl(imi_get_word()), data, (char)size);
      break;
    case EA_AIX:
      mem_set(ra[reg] + idx_get(), data, (char)size);
      break;
    case EA_SRT:
      mem_set(extl(imi_get_word()), data, (char)size);
      break;
    case EA_LNG:
      mem_set(imi_get(S_LONG), data, (char)size);
      break;
    case EA_PC:
      mem_set(save_pc + extl(imi_get_word()), data, (char)size);
      break;
    case EA_PCX:
      mem_set(save_pc + idx_get(), data, (char)size);
      break;
    default:
      err68a("アドレッシングモードが異常です。", __FILE__, __LINE__);
  }

  return false;
}

/*
 * 【説明】
 *   実効アドレスで示された値を取得する。
 *   この時、PCを移動させない。
 *
 * 【関数書式】
 *   retcode = get_data_at_ea_noinc(AceptAdrMode, mode, reg, &data);
 *
 * 【引数】
 *   int AceptAdrMode; <in>  処理可能なアドレッシングモード群 EA_????*
 *   int mode;         <in>  アドレッシングモード MD_??
 *   int reg;          <in>  レジスタ番号またはアドレッシングモード　MR_??
 *   Long *data;       <out> 取得するデータを格納する場所へのポインタ
 *
 * 【返値】
 *   true:  エラー
 *   false: 正常
 *
 */

bool get_data_at_ea_noinc(int AceptAdrMode, int mode, int reg, int size,
                          Long *data) {
  Long save_pc = pc;
  bool retcode = get_data_at_ea(AceptAdrMode, mode, reg, size, data);
  pc = save_pc;
  return retcode;
}

/* $Id: eaaccess.c,v 1.3 2009-08-08 06:49:44 masamic Exp $ */

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
 * Revision 1.6  2000/01/09  06:49:20  yfujii
 * Push/Pop instruction's word alignment is adjusted.
 *
 * Revision 1.3  1999/12/07  12:42:08  yfujii
 * *** empty log message ***
 *
 * Revision 1.3  1999/11/04  09:05:57  yfujii
 * Wrong addressing mode selection problem is fixed.
 *
 * Revision 1.1  1999/11/01  10:36:33  masamichi
 * Initial revision
 *
 *
 */
