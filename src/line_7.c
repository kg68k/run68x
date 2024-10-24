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

#include "mem.h"
#include "run68.h"

/*
 　機能：7ライン命令(moveq)を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool line7(char *pc_ptr) {
  char code = *(pc_ptr++);
  pc += 2;
  if ((code & 0x01) != 0) return IllegalInstruction();

  int reg = (code >> 1) & 0x07;
  rd[reg] = extbl((Byte)*pc_ptr);

  /* フラグの変化 */
  general_conditions(rd[reg], S_LONG);

#ifdef TRACE
  printf("trace: moveq    src=%d PC=%06lX\n", data, pc);
#endif

  return false;
}

/* $Id: line7.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:07  masamic
 * First imported source code and docs
 *
 * Revision 1.4  1999/12/07  12:44:50  yfujii
 * *** empty log message ***
 *
 * Revision 1.4  1999/11/22  03:57:08  yfujii
 * Condition code calculations are rewriten.
 *
 * Revision 1.3  1999/10/20  04:00:59  masamichi
 * Added showing more information about errors.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
