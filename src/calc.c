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

/* $Id: calc.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:05  masamic
 * First imported source code and docs
 *
 * Revision 1.3  1999/12/21  10:08:59  yfujii
 * Uptodate source code from Beppu.
 *
 * Revision 1.2  1999/12/07  12:39:26  yfujii
 * *** empty log message ***
 *
 * Revision 1.2  1999/10/21  12:17:46  yfujii
 * Added RCS keywords and modified for WIN32 a little.
 *
 */

#undef MAIN

#include <stdio.h>

#include "run68.h"

/*
 　機能：destにsrcをsizeサイズで加算する
 戻り値：答え
*/
Long add_long(Long src, Long dest, int size) {
  switch (size) {
    case S_BYTE:
      return (dest & 0xffffff00) | ((dest + src) & 0xff);
    case S_WORD:
      return (dest & 0xffff0000) | ((dest + src) & 0xffff);
    case S_LONG:
      return dest + src;

    default:
      err68a("不正なデータサイズです。", __FILE__, __LINE__);
  }
}

/*
 　機能：destからsrcをsizeサイズで減算する
 戻り値：答え
*/
Long sub_long(Long src, Long dest, int size) {
  switch (size) {
    case S_BYTE:
      return (dest & 0xffffff00) | ((dest - src) & 0xff);
    case S_WORD:
      return (dest & 0xffff0000) | ((dest - src) & 0xffff);
    case S_LONG:
      return dest - src;

    default:
      err68a("不正なデータサイズです。", __FILE__, __LINE__);
  }
}
