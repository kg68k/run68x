/* $Id: line6.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:07  masamic
 * First imported source code and docs
 *
 * Revision 1.6  1999/12/21  10:08:59  yfujii
 * Uptodate source code from Beppu.
 *
 * Revision 1.5  1999/12/07  12:44:37  yfujii
 * *** empty log message ***
 *
 * Revision 1.5  1999/11/22  03:57:08  yfujii
 * Condition code calculations are rewriten.
 *
 * Revision 1.3  1999/10/28  06:34:08  masamichi
 * Modified trace behavior
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */

#include <stdbool.h>
#include <stdio.h>

#include "operate.h"
#include "run68.h"

/*
 　機能：6ライン命令を実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool line6(char *pc_ptr) {
  int cond = (*pc_ptr & 0x0f);
  Byte disp8 = *(pc_ptr + 1);
  pc += 2;

  if (cond == 0x01) {  // bsr
    ra[7] -= 4;
    if (disp8 == 0) {
      Word disp16 = imi_get_word();
      mem_set(ra[7], pc, S_LONG);
      pc += extl(disp16) - 2;
    } else {
      mem_set(ra[7], pc, S_LONG);
      pc += extbl(disp8);
    }
    return false;
  }

  if (get_cond(cond)) {
    if (disp8 == 0) {
      Word disp16 = imi_get_word();
      pc += extl(disp16) - 2;
    } else {
      pc += extbl(disp8);
    }
  } else {
    // Bcc.W の分岐不成立ならワードディスプレースメントを飛ばす
    if (disp8 == 0) pc += 2;
  }

  return false;
}
