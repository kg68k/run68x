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

/* $Id: linef.c,v 1.3 2009-08-08 06:49:44 masamic Exp $ */

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
 * Revision 1.4  1999/12/07  12:46:15  yfujii
 * *** empty log message ***
 *
 * Revision 1.4  1999/10/22  04:02:00  yfujii
 * 'ltoa()'s are replaced by '_ltoa()'.
 *
 * Revision 1.3  1999/10/20  04:14:48  masamichi
 * Added showing more information about errors.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "run68.h"

typedef union {
  double dbl;
  UChar c[8];
} DBL;

typedef union {
  float flt;
  UChar c[4];
} FLT;

static int fefunc(UChar);
static Long Lmul(Long, Long);
static Long Ldiv(Long, Long);
static Long Lmod(Long, Long);
static ULong Umul(ULong, ULong);
static ULong Udiv(ULong, ULong);
static ULong Umod(ULong, ULong);
static Long Dtol(Long, Long);
static Long Ltof(Long);
static Long Ftol(Long);
static void Ftod(Long);
static Long Stol(Long);
static void Stod(Long);
static void Ltod(Long);
static void Dtos(Long, Long, Long);
static void Ltos(Long, Long);
static void Htos(Long, Long);
static void Otos(Long, Long);
static void Btos(Long, Long);
static void Val(Long);
static void Iusing(Long, Long, Long);
static void Using(Long, Long, Long, Long, Long, Long);
static void Dtst(Long, Long);
static void Dcmp(Long, Long, Long, Long);
static void Dneg(Long, Long);
static void Dadd(Long, Long, Long, Long);
static void Dsub(Long, Long, Long, Long);
static void Dmul(Long, Long, Long, Long);
static void Ddiv(Long, Long, Long, Long);
static void Dmod(Long, Long, Long, Long);
static void Dabs(Long, Long);
static void Dfloor(Long, Long);
static void Fcvt(Long, Long, Long, Long);
static void Sin(Long, Long);
static void Cos(Long, Long);
static void Tan(Long, Long);
static void Atan(Long, Long);
static void Log(Long, Long);
static void Exp(Long, Long);
static void Sqr(Long, Long);
static void Ftst(Long);
static Long Fmul(Long, Long);
static Long Fdiv(Long, Long);
static void Clmul(Long);
static void Cldiv(Long);
static void Clmod(Long);
static void Cumul(ULong);
static void Cudiv(ULong);
static void Cumod(ULong);
static void Cltod(Long);
static void Cdtol(Long);
static void Cftod(Long);
static void Cdtof(Long);
static void Cdadd(Long);
static void Cdcmp(Long);
static void Cdsub(Long);
static void Cdmul(Long);
static void Cddiv(Long);
static int Strl(char *, int);
static void From_dbl(DBL *, int);
static void To_dbl(DBL *, Long, Long);

static void Pow(Long, Long, Long, Long);

/*
 　機能：Fライン命令を実行する
 戻り値： TRUE = 実行終了
         FALSE = 実行継続
*/
int linef(char *pc_ptr) {
  char code;

  code = *(pc_ptr++);
  pc += 2;

  /* DOSコールの処理 */
  if (code == (char)0xFF) return (dos_call(*pc_ptr));

  /* FLOATコールの処理 */
  if (code == (char)0xFE) return (fefunc(*pc_ptr));

  err68a("未定義のＦライン命令を実行しました", __FILE__, __LINE__);
  return (TRUE);
}

/*
 　機能：FLOAT CALLを実行する
 戻り値： TRUE = 実行終了
         FALSE = 実行継続
*/
static int fefunc(UChar code) {
  Long adr;
  short save_s;

  /* F系列のベクタが書き換えられているかどうか検査 */
  save_s = SR_S_REF();
  SR_S_ON();
  adr = mem_get(0x2C, S_LONG);
  if (adr != HUMAN_WORK) {
    ra[7] -= 4;
    mem_set(ra[7], pc - 2, S_LONG);
    ra[7] -= 2;
    mem_set(ra[7], sr, S_WORD);
    pc = adr;
    return (FALSE);
  }
  if (save_s == 0) SR_S_OFF();

#ifdef TRACE
  printf("trace: FEFUNC   0xFE%02X PC=%06lX\n", code, pc);
#endif
  switch (code) {
    case 0x00:
      rd[0] = Lmul(rd[0], rd[1]);
      break;
    case 0x01:
      rd[0] = Ldiv(rd[0], rd[1]);
      break;
    case 0x02:
      rd[0] = Lmod(rd[0], rd[1]);
      break;
    case 0x04:
      rd[0] = Umul((ULong)rd[0], (ULong)rd[1]);
      break;
    case 0x05:
      rd[0] = Udiv((ULong)rd[0], (ULong)rd[1]);
      break;
    case 0x06:
      rd[0] = Umod((ULong)rd[0], (ULong)rd[1]);
      break;
    case 0x08: /* _IMUL */
      rd[1] = (ULong)rd[0] * (ULong)rd[1];
      if (rd[1] < 0)
        rd[0] = -1; /* 本当は上位4バイトが入る */
      else
        rd[0] = 0; /* 本当は上位4バイトが入る */
      break;
    case 0x09: /* _IDIV */ /* unsigned int 除算 d0..d1 d0/d1 */
    {
      ULong d0;
      ULong d1;

      d0 = (ULong)rd[0];
      d1 = (ULong)rd[1];

      rd[0] = Udiv(d0, d1);
      rd[1] = Umod(d0, d1);
    } break;
    case 0x0C: /* _RANDOMIZE */
      if (rd[0] >= -32768 && rd[0] <= 32767) srand(rd[0] + 32768);
      break;
    case 0x0D: /* _SRAND */
      if (rd[0] >= 0 && rd[0] <= 65535) srand(rd[0]);
      break;
    case 0x0E: /* _RAND */
      rd[0] = ((unsigned)(rand()) % 32768);
      break;
    case 0x10:
      rd[0] = Stol(ra[0]);
      break;
    case 0x11:
      Ltos(rd[0], ra[0]);
      break;
    case 0x13:
      Htos(rd[0], ra[0]);
      break;
    case 0x15:
      Otos(rd[0], ra[0]);
      break;
    case 0x17:
      Btos(rd[0], ra[0]);
      break;
    case 0x18:
      Iusing(rd[0], rd[1], ra[0]);
      break;
    case 0x1A:
      Ltod(rd[0]);
      break;
    case 0x1B:
      rd[0] = Dtol(rd[0], rd[1]);
      break;
    case 0x1C:
      rd[0] = Ltof(rd[0]);
      break;
    case 0x1D:
      rd[0] = Ftol(rd[0]);
      break;
    case 0x1E:
      Ftod(rd[0]);
      break;
    case 0x20:
      Val(ra[0]);
      break;
    case 0x21:
      Using(rd[0], rd[1], rd[2], rd[3], rd[4], ra[0]);
      break;
    case 0x22:
      Stod(ra[0]);
      break;
    case 0x23:
      Dtos(rd[0], rd[1], ra[0]);
      break;
    case 0x25:
      Fcvt(rd[0], rd[1], rd[2], ra[0]);
      break;
    case 0x28:
      Dtst(rd[0], rd[1]);
      break;
    case 0x29:
      Dcmp(rd[0], rd[1], rd[2], rd[3]);
      break;
    case 0x2A:
      Dneg(rd[0], rd[1]);
      break;
    case 0x2B:
      Dadd(rd[0], rd[1], rd[2], rd[3]);
      break;
    case 0x2C:
      Dsub(rd[0], rd[1], rd[2], rd[3]);
      break;
    case 0x2D:
      Dmul(rd[0], rd[1], rd[2], rd[3]);
      break;
    case 0x2E:
      Ddiv(rd[0], rd[1], rd[2], rd[3]);
      break;
    case 0x2F:
      Dmod(rd[0], rd[1], rd[2], rd[3]);
      break;
    case 0x30:
      Dabs(rd[0], rd[1]);
      break;
    case 0x33:
      Dfloor(rd[0], rd[1]);
      break;
    case 0x36:
      Sin(rd[0], rd[1]);
      break;
    case 0x37:
      Cos(rd[0], rd[1]);
      break;
    case 0x38:
      Tan(rd[0], rd[1]);
      break;
    case 0x39:
      Atan(rd[0], rd[1]);
      break;
    case 0x3A:
      Log(rd[0], rd[1]);
      break;
    case 0x3B:
      Exp(rd[0], rd[1]);
      break;
    case 0x3C:
      Sqr(rd[0], rd[1]);
      break;
    case 0x3F:
      Pow(rd[0], rd[1], rd[2], rd[3]);
      break;
    case 0x40: /* _RND */
      rd[0] = rand() * rand() * 4;
      rd[1] = rand() * rand() * 4;
      break;
    case 0x58:
      Ftst(rd[0]);
      break;
    case 0x5D:
      rd[0] = Fmul(rd[0], rd[1]);
      break;
    case 0x5E:
      rd[0] = Fdiv(rd[0], rd[1]);
      break;
      /*
                      case 0x6C:
                              rd [ 0 ] = Fsqr( rd [ 0 ] );
                              break;
      */
    case 0xE0: /* __CLMUL : signed int 乗算 */
      Clmul(ra[7]);
      break;
    case 0xE1: /* __CLDIV : signed int 除算 */
      Cldiv(ra[7]);
      break;
    case 0xE2: /* __CLMOD : signed int 除算の剰余 */
      Clmod(ra[7]);
      break;
    case 0xE3: /* __CUMUL : unsigned int 乗算 */
      Cumul(ra[7]);
      break;
    case 0xE4: /* __CUDIV : unsigned int 除算 */
      Cudiv(ra[7]);
      break;
    case 0xE5: /* __CUMOD : unsigned int 除算の剰余 */
      Cumod(ra[7]);
      break;
    case 0xE6:
      Cltod(ra[7]);
      break;
    case 0xE7:
      Cdtol(ra[7]);
      break;
    case 0xEA:
      Cftod(ra[7]);
      break;
    case 0xEB:
      Cdtof(ra[7]);
      break;
    case 0xEC:
      Cdcmp(ra[7]);
      break;
    case 0xED:
      Cdadd(ra[7]);
      break;
    case 0xEE:
      Cdsub(ra[7]);
      break;
    case 0xEF:
      Cdmul(ra[7]);
      break;
    case 0xF0:
      Cddiv(ra[7]);
      break;
    default:
      printf("0x%X\n", code);
      err68a("未登録のFEファンクションコールを実行しました", __FILE__,
             __LINE__);
      return (TRUE);
  }

  return (FALSE);
}

/*
 　機能：FEFUNC _LMULを実行する(エラーは未サポート)
 戻り値：演算結果
*/
static Long Lmul(Long d0, Long d1) { return (d0 * d1); }

/*
 　機能：FEFUNC _LDIVを実行する
 戻り値：演算結果
*/
static Long Ldiv(Long d0, Long d1) {
  if (d1 == 0) {
    CCR_C_ON();
    return (0);
  }

  CCR_C_OFF();
  return (d0 / d1);
}

/*
 　機能：FEFUNC _LMODを実行する
 戻り値：演算結果
*/
static Long Lmod(Long d0, Long d1) {
  if (d1 == 0) {
    CCR_C_ON();
    return (0);
  }

  CCR_C_OFF();
  return (d0 % d1);
}

/*
 　機能：FEFUNC _UMULを実行する(エラーは未サポート)
 戻り値：演算結果
*/
static ULong Umul(ULong d0, ULong d1) { return (d0 * d1); }

/*
 　機能：FEFUNC _UDIVを実行する
 戻り値：演算結果
*/
static ULong Udiv(ULong d0, ULong d1) {
  if (d1 == 0) {
    CCR_C_ON();
    return (0);
  }

  CCR_C_OFF();
  return (d0 / d1);
}

/*
 　機能：FEFUNC _UMODを実行する
 戻り値：演算結果
*/
static ULong Umod(ULong d0, ULong d1) {
  if (d1 == 0) {
    CCR_C_ON();
    return (0);
  }

  CCR_C_OFF();
  return (d0 % d1);
}

/*
 　機能：FEFUNC _DTOLを実行する(エラーは未サポート)
 戻り値：変換された整数
*/
static Long Dtol(Long d0, Long d1) {
  DBL arg1;

  To_dbl(&arg1, d0, d1);

  return ((Long)arg1.dbl);
}

/*
 　機能：FEFUNC _LTOFを実行する
 戻り値：なし
*/
static Long Ltof(Long d0) {
  FLT fl;

  fl.flt = (float)d0;

  d0 = (fl.c[3] << 24);
  d0 |= (fl.c[2] << 16);
  d0 |= (fl.c[1] << 8);
  d0 |= fl.c[0];

  return (d0);
}

/*
 　機能：FEFUNC _FTOLを実行する(エラーは未サポート)
 戻り値：変換された整数
*/
static Long Ftol(Long d0) {
  FLT fl;

  fl.c[0] = (d0 & 0xFF);
  fl.c[1] = ((d0 >> 8) & 0xFF);
  fl.c[2] = ((d0 >> 16) & 0xFF);
  fl.c[3] = ((d0 >> 24) & 0xFF);

  return ((Long)fl.flt);
}

/*
 　機能：FEFUNC _FTODを実行する
 戻り値：なし
*/
static void Ftod(Long d0) {
  DBL ret;
  FLT arg;

  arg.c[0] = (d0 & 0xFF);
  arg.c[1] = ((d0 >> 8) & 0xFF);
  arg.c[2] = ((d0 >> 16) & 0xFF);
  arg.c[3] = ((d0 >> 24) & 0xFF);

  ret.dbl = arg.flt;

  From_dbl(&ret, 0);
}

/*
 　機能：FEFUNC _STOLを実行する
 戻り値：変換された整数
*/
static Long Stol(Long adr) {
  char *p;
  Long ret;

  p = prog_ptr + adr;
  errno = 0;
  ret = strtol(p, NULL, 10);
  if (ret == 0) {
    if (errno == EINVAL) {
      CCR_C_ON();
      CCR_N_ON();
      CCR_V_OFF();
    } else {
      CCR_C_OFF();
      ra[0] += Strl(p, 10);
    }
  } else {
    if (errno == ERANGE) {
      CCR_C_ON();
      CCR_N_OFF();
      CCR_V_ON();
    } else {
      CCR_C_OFF();
      ra[0] += Strl(p, 10);
    }
  }
  return (ret);
}

/*
 　機能：FEFUNC _STODを実行する
 戻り値：なし
*/
static void Stod(Long adr) {
  char *p;
  DBL ret;

  p = prog_ptr + adr;
  errno = 0;
  ret.dbl = atof(p);
  if (errno == ERANGE) {
    CCR_C_ON();
    CCR_N_OFF();
    CCR_V_ON();
  } else {
    CCR_C_OFF();
    ra[0] += Strl(p, 10);
  }

  From_dbl(&ret, 0);

  if (ret.dbl == (Long)ret.dbl) {
    rd[2] |= 0xFFFF;
    rd[3] = (Long)ret.dbl;
  } else {
    rd[2] &= 0xFFFF0000;
  }
}

/*
 　機能：FEFUNC _LTODを実行する
 戻り値：なし
*/
static void Ltod(Long num) {
  DBL arg1;

  arg1.dbl = num;

  From_dbl(&arg1, 0);
}

/*
 　機能：FEFUNC _DTOSを実行する
 戻り値：なし
*/
static void Dtos(Long d0, Long d1, Long a0) {
  DBL arg1;

  To_dbl(&arg1, d0, d1);
  char *p = prog_ptr + a0;
  sprintf(p, "%.14g", arg1.dbl);
  ra[0] += strlen(p);
}

/*
 　機能：FEFUNC _LTOSを実行する
 戻り値：なし
*/
static void Ltos(Long num, Long adr) {
  char *p;

  p = prog_ptr + adr;
  sprintf(p, "%d", num);
  ra[0] += strlen(p);
}

/*
 　機能：FEFUNC _HTOSを実行する
 戻り値：なし
*/
static void Htos(Long num, Long adr) {
  char *p;

  p = prog_ptr + adr;
  sprintf(p, "%X", num);
  ra[0] += strlen(p);
}

/*
 　機能：FEFUNC _OTOSを実行する
 戻り値：なし
*/
static void Otos(Long num, Long adr) {
  char *p;

  p = prog_ptr + adr;
  sprintf(p, "%o", num);
  ra[0] += strlen(p);
}

/*
 　機能：FEFUNC _BTOSを実行する
 戻り値：なし
*/
static void Btos(Long num, Long adr) {
  char *const buffer = prog_ptr + adr;
  char *p = buffer;

  if (num == 0) {
    *p++ = '0';
  } else {
    ULong mask = 1u << 31;
    for (; mask; mask >>= 1) {  // 上位桁の0は出力しない
      if ((num & mask) != 0) break;
    }
    for (; mask; mask >>= 1) {
      *p++ = (num & mask) ? '1' : '0';
    }
  }
  *p = '\0';
  ra[0] += buffer - p;  // a0は出力文字列末尾の\0を指す
}

/*
 　機能：FEFUNC _VALを実行する
 戻り値：なし
*/
static void Val(Long str) {
  DBL ret;
  char *p = prog_ptr + str;
  int base = 10;

  if (p[0] == '&') {
    char c = toupper(p[1]);
    if (c == 'H')
      base = 16;
    else if (c == 'O')
      base = 8;
    else if (c == 'B')
      base = 2;
    else {
      CCR_C_ON();
      CCR_N_ON();
      CCR_V_OFF();
      return;
    }
    errno = 0;
    unsigned long ul = strtoul(p + 2, NULL, base);
    if (ul == ULONG_MAX && errno == ERANGE) {
      CCR_C_ON();
      CCR_N_OFF();
      CCR_V_ON();
      return;
    }
    ret.dbl = (double)ul;
  } else {
    errno = 0;
    ret.dbl = strtod(p, NULL);
    if (errno != 0) {
      CCR_C_ON();
      CCR_N_OFF();
      CCR_V_ON();
      return;
    }
  }
  ra[0] += Strl((base == 10) ? p : p + 2, base);

  From_dbl(&ret, 0);

  if (base == 10 && ret.dbl == (Long)ret.dbl) {
    rd[2] |= 0xFFFF;
    rd[3] = (Long)ret.dbl;
  } else {
    rd[2] &= 0xFFFF0000;
  }
  CCR_C_OFF();
}

/*
 　機能：FEFUNC _IUSINGを実行する
 戻り値：なし
*/
static void Iusing(Long num, Long keta, Long adr) {
  char form1[] = {"%1d"};
  char form2[] = {"%10d"};
  char *p;

  p = prog_ptr + adr;
  keta = (ULong)keta % 100;
  if (keta == 0) return;

  if (keta < 10) {
    form1[1] = keta + '0';
    sprintf(p, form1, num);
  } else {
    form2[1] = keta / 10 + '0';
    form2[2] = keta % 10 + '0';
    sprintf(p, form2, num);
  }
  ra[0] += strlen(p);
}

/*
 　機能：FEFUNC _USINGを実行する(アトリビュート一部未対応)
 戻り値：なし
*/
static void Using(Long d0, Long d1, Long isz, Long dsz, Long atr, Long a0) {
  char str[128];
  DBL arg1;
  char form1[] = {"%1.1f"};
  char form2[] = {"%1.10f"};
  char form3[] = {"%10.1f"};
  char form4[] = {"%10.10f"};
  char *p;
  char *p2;

  To_dbl(&arg1, d0, d1);

  isz = (ULong)isz % 100;
  if (isz == 0) return;

  if (dsz <= 0) {
    dsz = 0;
  } else {
    dsz = (ULong)dsz % 100;
    isz += dsz + 1; /* 1 = strlen( "." ) */
    isz = (ULong)isz % 100;
    if (isz == 0) return;
  }

  p = prog_ptr + a0;
  if (isz < 10) {
    if (dsz < 10) {
      form1[1] = isz + '0';
      form1[3] = dsz + '0';
      sprintf(p, form1, arg1.dbl);
    } else {
      form2[1] = isz + '0';
      form2[3] = dsz / 10 + '0';
      form2[4] = dsz % 10 + '0';
      sprintf(p, form2, arg1.dbl);
    }
  } else {
    if (dsz < 10) {
      form3[1] = isz / 10 + '0';
      form3[2] = isz % 10 + '0';
      form3[4] = dsz + '0';
      sprintf(p, form3, arg1.dbl);
    } else {
      form4[1] = isz / 10 + '0';
      form4[2] = isz % 10 + '0';
      form4[4] = dsz / 10 + '0';
      form4[5] = dsz % 10 + '0';
      sprintf(p, form4, arg1.dbl);
    }
  }

  /* bit5or6が立っていたら'-'を取る */
  if ((atr & 0x60) != 0 && arg1.dbl < 0) {
    if (*p == '-' && (Long)strlen(p) > isz) {
      strcpy(str, p + 1);
      strcpy(p, str);
    } else {
      p2 = p;
      while (*p2 == ' ') p2++;
      if (*p2 == '-') /* 念のため */
        *p2 = ' ';
    }
  }

  /* '\'を先頭に付加 */
  if ((atr & 0x02) != 0) {
    p2 = p;
    str[0] = '\0';
    while (*p2 == ' ') {
      if (p2 != p) strcat(str, " ");
      p2++;
    }
    if (*p2 == '-') {
      strcat(str, "-");
      *p2 = '\\';
    } else {
      strcat(str, "\\");
    }
    strcat(str, p2);
    strcpy(p, str);
  }

  /* 正の場合'+'を先頭に付加 */
  if ((atr & 0x10) != 0 && arg1.dbl >= 0) {
    strcpy(str, "+");
    strcat(str, p);
    strcpy(p, str);
  }

  /* 符号を末尾に付加 */
  if ((atr & 0x20) != 0) {
    if (arg1.dbl < 0)
      strcat(p, "-");
    else
      strcat(p, "+");
  }

  /* 負の場合'-'を、正の場合スペースを末尾に付加 */
  if ((atr & 0x40) != 0) {
    if (arg1.dbl < 0)
      strcat(p, "-");
    else
      strcat(p, " ");
  }

  ra[0] += strlen(p);
}

/*
 　機能：FEFUNC _DTSTを実行する
 戻り値：なし
*/
static void Dtst(Long d0, Long d1) {
  DBL arg;

  To_dbl(&arg, d0, d1);

  if (arg.dbl == 0) {
    CCR_Z_ON();
    CCR_N_OFF();
  } else if (arg.dbl < 0) {
    CCR_Z_OFF();
    CCR_N_ON();
  } else {
    CCR_Z_OFF();
    CCR_N_OFF();
  }
}

/*
 　機能：FEFUNC _DCMPを実行する
 戻り値：なし
*/
static void Dcmp(Long d0, Long d1, Long d2, Long d3) {
  DBL arg1;
  DBL arg2;

  To_dbl(&arg1, d0, d1);
  To_dbl(&arg2, d2, d3);

  arg1.dbl = arg1.dbl - arg2.dbl;

  if (arg1.dbl < 0) {
    CCR_C_ON();
    CCR_Z_OFF();
    CCR_N_ON();
  } else if (arg1.dbl > 0) {
    CCR_C_OFF();
    CCR_Z_OFF();
    CCR_N_OFF();
  } else {
    CCR_C_OFF();
    CCR_Z_ON();
    CCR_N_OFF();
  }
}

/*
 　機能：FEFUNC _DNEGを実行する
 戻り値：なし
*/
static void Dneg(Long d0, Long d1) {
  DBL arg1;

  To_dbl(&arg1, d0, d1);

  arg1.dbl = -arg1.dbl;

  From_dbl(&arg1, 0);
}

/*
 　機能：FEFUNC _DADDを実行する
 戻り値：なし
*/
static void Dadd(Long d0, Long d1, Long d2, Long d3) {
  DBL arg1;
  DBL arg2;

  To_dbl(&arg1, d0, d1);
  To_dbl(&arg2, d2, d3);

  CCR_C_OFF();
  arg1.dbl = arg1.dbl + arg2.dbl;

  From_dbl(&arg1, 0);
}

/*
 　機能：FEFUNC _DSUBを実行する
 戻り値：なし
*/
static void Dsub(Long d0, Long d1, Long d2, Long d3) {
  DBL arg1;
  DBL arg2;

  To_dbl(&arg1, d0, d1);
  To_dbl(&arg2, d2, d3);

  CCR_C_OFF();
  arg1.dbl = arg1.dbl - arg2.dbl;

  From_dbl(&arg1, 0);
}

/*
 　機能：FEFUNC _DMULを実行する
 戻り値：なし
*/
static void Dmul(Long d0, Long d1, Long d2, Long d3) {
  DBL arg1;
  DBL arg2;

  To_dbl(&arg1, d0, d1);
  To_dbl(&arg2, d2, d3);

  CCR_C_OFF();
  arg1.dbl = arg1.dbl * arg2.dbl;

  From_dbl(&arg1, 0);
}

/*
 　機能：FEFUNC _DDIVを実行する
 戻り値：なし
*/
static void Ddiv(Long d0, Long d1, Long d2, Long d3) {
  DBL arg1;
  DBL arg2;

  To_dbl(&arg1, d0, d1);
  To_dbl(&arg2, d2, d3);

  if (arg2.dbl == 0) {
    CCR_C_ON();
    CCR_Z_ON();
    return;
  }

  CCR_C_OFF();
  arg1.dbl = arg1.dbl / arg2.dbl;

  From_dbl(&arg1, 0);
}

/*
 　機能：FEFUNC _DMODを実行する
 戻り値：なし
*/
static void Dmod(Long d0, Long d1, Long d2, Long d3) {
  DBL arg1;
  DBL arg2;

  To_dbl(&arg1, d0, d1);
  To_dbl(&arg2, d2, d3);

  if (arg2.dbl == 0) {
    CCR_C_ON();
    CCR_Z_ON();
    return;
  }

  CCR_C_OFF();
  arg1.dbl = fmod(arg1.dbl, arg2.dbl);

  From_dbl(&arg1, 0);
}

/*
 　機能：FEFUNC _DABSを実行する
 戻り値：なし
*/
static void Dabs(Long d0, Long d1) {
  DBL arg1;

  To_dbl(&arg1, d0, d1);

  arg1.dbl = fabs(arg1.dbl);

  From_dbl(&arg1, 0);
}

/*
 　機能：FEFUNC _DFLOORを実行する
 戻り値：なし
*/
static void Dfloor(Long d0, Long d1) {
  DBL arg1;

  To_dbl(&arg1, d0, d1);

  arg1.dbl = floor(arg1.dbl);

  From_dbl(&arg1, 0);
}

/*
 　機能：FEFUNC _FCVTを実行する
 戻り値：なし
*/
static void Fcvt(Long d0, Long d1, Long keta, Long adr) {
  DBL arg;
  char *p;
  int loc;
  int sign;

  To_dbl(&arg, d0, d1);
  p = prog_ptr + adr;
  keta &= 0xFF;

  strcpy(p, (char *)_fcvt(arg.dbl, keta, &loc, &sign));

  rd[0] = loc;
  if (sign == 0)
    rd[1] = 0;
  else
    rd[1] = 1;
}

/*
 　機能：FEFUNC _SINを実行する
 戻り値：なし
*/
static void Sin(Long d0, Long d1) {
  DBL arg;
  DBL ans;

  To_dbl(&arg, d0, d1);

  ans.dbl = sin(arg.dbl);

  From_dbl(&ans, 0);
}

/*
 　機能：FEFUNC _COSを実行する
 戻り値：なし
*/
static void Cos(Long d0, Long d1) {
  DBL arg;
  DBL ans;

  To_dbl(&arg, d0, d1);

  ans.dbl = cos(arg.dbl);

  From_dbl(&ans, 0);
}

/*
 　機能：FEFUNC _TANを実行する
 戻り値：なし
*/
static void Tan(Long d0, Long d1) {
  DBL arg;
  DBL ans;

  To_dbl(&arg, d0, d1);

  ans.dbl = tan(arg.dbl);
  CCR_C_OFF();

  From_dbl(&ans, 0);
}

/*
 　機能：FEFUNC _ATANを実行する
 戻り値：なし
*/
static void Atan(Long d0, Long d1) {
  DBL arg;
  DBL ans;

  To_dbl(&arg, d0, d1);

  ans.dbl = atan(arg.dbl);

  From_dbl(&ans, 0);
}

/*
 　機能：FEFUNC _LOGを実行する
 戻り値：なし
*/
static void Log(Long d0, Long d1) {
  DBL arg;
  DBL ans;

  To_dbl(&arg, d0, d1);

  if (arg.dbl == 0) {
    CCR_C_ON();
    CCR_Z_ON();
    return;
  }

  ans.dbl = log(arg.dbl);
  CCR_C_OFF();

  From_dbl(&ans, 0);
}

/*
 　機能：FEFUNC _EXPを実行する
 戻り値：なし
*/
static void Exp(Long d0, Long d1) {
  DBL arg;
  DBL ans;

  To_dbl(&arg, d0, d1);

  if (arg.dbl > 709.782712893) {
    CCR_C_ON();
    CCR_Z_OFF();
    CCR_V_ON();
    return;
  }

  errno = 0;
  ans.dbl = exp(arg.dbl);
  CCR_C_OFF();

  From_dbl(&ans, 0);
}

/*
 　機能：FEFUNC _SQRを実行する
 戻り値：なし
*/
static void Sqr(Long d0, Long d1) {
  DBL arg;
  DBL ans;

  To_dbl(&arg, d0, d1);

  if (arg.dbl < 0) {
    CCR_C_ON();
    return;
  }
  ans.dbl = sqrt(arg.dbl);
  CCR_C_OFF();

  From_dbl(&ans, 0);
}

/*
 　機能：FEFUNC _FTSTを実行する
 戻り値：なし
*/
static void Ftst(Long d0) {
  FLT arg;

  arg.c[0] = (d0 & 0xFF);
  arg.c[1] = ((d0 >> 8) & 0xFF);
  arg.c[2] = ((d0 >> 16) & 0xFF);
  arg.c[3] = ((d0 >> 24) & 0xFF);

  if (arg.flt == 0) {
    CCR_Z_ON();
    CCR_N_OFF();
  } else if (arg.flt < 0) {
    CCR_Z_OFF();
    CCR_N_ON();
  } else {
    CCR_Z_OFF();
    CCR_N_OFF();
  }
}

/*
 　機能：FEFUNC _FMULを実行する＜エラーは未サポート＞
 戻り値：演算結果
*/
static Long Fmul(Long d0, Long d1) {
  FLT arg1;
  FLT arg2;

  arg1.c[0] = (d0 & 0xFF);
  arg1.c[1] = ((d0 >> 8) & 0xFF);
  arg1.c[2] = ((d0 >> 16) & 0xFF);
  arg1.c[3] = ((d0 >> 24) & 0xFF);

  arg2.c[0] = (d1 & 0xFF);
  arg2.c[1] = ((d1 >> 8) & 0xFF);
  arg2.c[2] = ((d1 >> 16) & 0xFF);
  arg2.c[3] = ((d1 >> 24) & 0xFF);

  CCR_C_OFF();
  arg1.flt = arg1.flt * arg2.flt;

  d0 = (arg1.c[3] << 24);
  d0 |= (arg1.c[2] << 16);
  d0 |= (arg1.c[1] << 8);
  d0 |= arg1.c[0];

  return (d0);
}

/*
 　機能：FEFUNC _FDIVを実行する
 戻り値：なし
*/
static Long Fdiv(Long d0, Long d1) {
  FLT arg1;
  FLT arg2;

  arg1.c[0] = (d0 & 0xFF);
  arg1.c[1] = ((d0 >> 8) & 0xFF);
  arg1.c[2] = ((d0 >> 16) & 0xFF);
  arg1.c[3] = ((d0 >> 24) & 0xFF);

  arg2.c[0] = (d1 & 0xFF);
  arg2.c[1] = ((d1 >> 8) & 0xFF);
  arg2.c[2] = ((d1 >> 16) & 0xFF);
  arg2.c[3] = ((d1 >> 24) & 0xFF);

  if (arg2.flt == 0) {
    CCR_C_ON();
    CCR_Z_ON();
    return (0);
  }

  CCR_C_OFF();
  arg1.flt = arg1.flt / arg2.flt;

  d0 = (arg1.c[3] << 24);
  d0 |= (arg1.c[2] << 16);
  d0 |= (arg1.c[1] << 8);
  d0 |= arg1.c[0];

  return (d0);
}

/*
 　機能：FEFUNC _CLMULを実行する(エラーは未サポート)
 戻り値：なし
*/
static void Clmul(Long adr) {
  Long a;
  Long b;

  a = mem_get(adr, S_LONG);
  b = mem_get(adr + 4, S_LONG);

  a = a * b;
  CCR_C_OFF();

  mem_set(adr, a, S_LONG);
}

/*
 　機能：FEFUNC _CLDIVを実行する
 戻り値：なし
*/
static void Cldiv(Long adr) {
  Long a;
  Long b;

  a = mem_get(adr, S_LONG);
  b = mem_get(adr + 4, S_LONG);

  if (b == 0) {
    CCR_C_ON();
    return;
  }

  a = a / b;
  CCR_C_OFF();

  mem_set(adr, a, S_LONG);
}

/*
 　機能：FEFUNC _CLMODを実行する
 戻り値：なし
*/
static void Clmod(Long adr) {
  Long a;
  Long b;

  a = mem_get(adr, S_LONG);
  b = mem_get(adr + 4, S_LONG);

  if (b == 0) {
    CCR_C_ON();
    return;
  }

  a = a % b;
  CCR_C_OFF();

  mem_set(adr, a, S_LONG);
}

/*
 　機能：FEFUNC _CUMULを実行する(エラーは未サポート)
 戻り値：なし
*/
static void Cumul(ULong adr) {
  ULong a;
  ULong b;

  a = mem_get(adr, S_LONG);
  b = mem_get(adr + 4, S_LONG);

  a = a * b;
  CCR_C_OFF();

  mem_set(adr, a, S_LONG);
}

/*
 　機能：FEFUNC _CUDIVを実行する
 戻り値：なし
*/
static void Cudiv(ULong adr) {
  ULong a;
  ULong b;

  a = mem_get(adr, S_LONG);
  b = mem_get(adr + 4, S_LONG);

  if (b == 0) {
    CCR_C_ON();
    return;
  }

  a = a / b;
  CCR_C_OFF();

  mem_set(adr, a, S_LONG);
}

/*
 　機能：FEFUNC _CUMODを実行する
 戻り値：なし
*/
static void Cumod(ULong adr) {
  ULong a;
  ULong b;

  a = mem_get(adr, S_LONG);
  b = mem_get(adr + 4, S_LONG);

  if (b == 0) {
    CCR_C_ON();
    return;
  }

  a = a % b;
  CCR_C_OFF();

  mem_set(adr, a, S_LONG);
}

/*
 　機能：FEFUNC _CLTODを実行する
 戻り値：なし
*/
static void Cltod(Long adr) {
  DBL arg1;
  Long num;
  Long d0;
  Long d1;

  num = mem_get(adr, S_LONG);
  arg1.dbl = num;

  d0 = rd[0];
  d1 = rd[1];
  From_dbl(&arg1, 0);
  mem_set(adr, rd[0], S_LONG);
  mem_set(adr + 4, rd[1], S_LONG);
  rd[0] = d0;
  rd[1] = d1;
}

/*
 　機能：FEFUNC _CDTOLを実行する(エラーは未サポート)
 戻り値：なし
*/
static void Cdtol(Long adr) {
  DBL arg1;
  Long d0;
  Long d1;

  d0 = mem_get(adr, S_LONG);
  d1 = mem_get(adr + 4, S_LONG);
  To_dbl(&arg1, d0, d1);

  d0 = (Long)arg1.dbl;

  mem_set(adr, d0, S_LONG);
}

/*
 　機能：FEFUNC _CFTODを実行する
 戻り値：なし
*/
static void Cftod(Long adr) {
  DBL db;
  FLT fl;
  Long d0;
  Long d1;

  d0 = mem_get(adr, S_LONG);
  fl.c[0] = (d0 & 0xFF);
  fl.c[1] = ((d0 >> 8) & 0xFF);
  fl.c[2] = ((d0 >> 16) & 0xFF);
  fl.c[3] = ((d0 >> 24) & 0xFF);

  db.dbl = fl.flt;

  d0 = rd[0];
  d1 = rd[1];
  From_dbl(&db, 0);
  mem_set(adr, rd[0], S_LONG);
  mem_set(adr + 4, rd[1], S_LONG);
  rd[0] = d0;
  rd[1] = d1;
}

/*
 　機能：FEFUNC _CDTOFを実行する
 戻り値：なし
*/
static void Cdtof(Long adr) {
  DBL arg;
  FLT fl;
  Long d0;
  Long d1;

  d0 = rd[0];
  d1 = rd[1];
  rd[0] = mem_get(adr, S_LONG);
  rd[1] = mem_get(adr + 4, S_LONG);
  To_dbl(&arg, d0, d1);
  rd[0] = d0;
  rd[1] = d1;

  fl.flt = (float)arg.dbl;
  CCR_C_OFF();

  d0 = (fl.c[3] << 24);
  d0 |= (fl.c[2] << 16);
  d0 |= (fl.c[1] << 8);
  d0 |= fl.c[0];
  mem_set(adr, d0, S_LONG);
}

/*
 　機能：FEFUNC _CDCMPを実行する
 戻り値：なし
*/
static void Cdcmp(Long adr) {
  DBL arg1;
  DBL arg2;
  Long d0;
  Long d1;

  d0 = rd[0];
  d1 = rd[1];
  rd[0] = mem_get(adr, S_LONG);
  rd[1] = mem_get(adr + 4, S_LONG);
  To_dbl(&arg1, d0, d1);
  rd[0] = mem_get(adr + 8, S_LONG);
  rd[1] = mem_get(adr + 12, S_LONG);
  To_dbl(&arg2, d0, d1);
  rd[0] = d0;
  rd[1] = d1;

  arg1.dbl = arg1.dbl - arg2.dbl;

  if (arg1.dbl < 0) {
    CCR_C_ON();
    CCR_Z_OFF();
    CCR_N_ON();
  } else if (arg1.dbl > 0) {
    CCR_C_OFF();
    CCR_Z_OFF();
    CCR_N_OFF();
  } else {
    CCR_C_OFF();
    CCR_Z_ON();
    CCR_N_OFF();
  }
}

/*
 　機能：FEFUNC _CDADDを実行する
 戻り値：なし
*/
static void Cdadd(Long adr) {
  DBL a;
  DBL b;
  int i;

  for (i = 0; i < 8; i++) {
    a.c[i] = (unsigned char)mem_get(adr + 7 - i, S_BYTE);
    b.c[i] = (unsigned char)mem_get(adr + 15 - i, S_BYTE);
  }

  a.dbl = a.dbl + b.dbl;

  for (i = 0; i < 8; i++) mem_set(adr + 7 - i, a.c[i], S_BYTE);
}

/*
 　機能：FEFUNC _CDSUBを実行する
 戻り値：なし
*/
static void Cdsub(Long adr) {
  DBL a;
  DBL b;
  int i;

  for (i = 0; i < 8; i++) {
    a.c[i] = (unsigned char)mem_get(adr + 7 - i, S_BYTE);
    b.c[i] = (unsigned char)mem_get(adr + 15 - i, S_BYTE);
  }

  a.dbl = a.dbl - b.dbl;

  for (i = 0; i < 8; i++) mem_set(adr + 7 - i, a.c[i], S_BYTE);
}

/*
 　機能：FEFUNC _CDMULを実行する
 戻り値：なし
*/
static void Cdmul(Long adr) {
  DBL a;
  DBL b;
  int i;

  for (i = 0; i < 8; i++) {
    a.c[i] = (unsigned char)mem_get(adr + 7 - i, S_BYTE);
    b.c[i] = (unsigned char)mem_get(adr + 15 - i, S_BYTE);
  }

  a.dbl = a.dbl * b.dbl;

  for (i = 0; i < 8; i++) mem_set(adr + 7 - i, a.c[i], S_BYTE);
}

/*
 　機能：FEFUNC _CDDIVを実行する
 戻り値：なし
*/
static void Cddiv(Long adr) {
  DBL a;
  DBL b;
  int i;

  for (i = 0; i < 8; i++) {
    a.c[i] = (unsigned char)mem_get(adr + 7 - i, S_BYTE);
    b.c[i] = (unsigned char)mem_get(adr + 15 - i, S_BYTE);
  }

  if (b.dbl == 0) {
    CCR_C_ON();
    CCR_Z_ON();
    return;
  }

  CCR_C_OFF();
  a.dbl = a.dbl / b.dbl;

  for (i = 0; i < 8; i++) mem_set(adr + 7 - i, a.c[i], S_BYTE);
}

/*
 　機能：数字文字列の数字以外の部分までの長さを求める
 戻り値：長さ
*/
static int Strl(char *p, int base) {
  int l;

  for (l = 0; p[l] == ' '; l++)
    ;
  switch (base) {
    case 10:
      for (; p[l] != '\0'; l++) {
        if (p[l] >= '0' && p[l] <= '9') continue;
        if (p[l] == '.') continue;
        break;
      }
      break;
    case 2:
      for (; p[l] != '\0'; l++) {
        if (p[l] != '0' && p[l] != '1') break;
      }
      break;
    case 8:
      for (; p[l] != '\0'; l++) {
        if (p[l] < '0' || p[l] > '7') break;
      }
      break;
    case 16:
      for (; p[l] != '\0'; l++) {
        if (p[l] >= '0' && p[l] <= '9') continue;
        if (p[l] >= 'A' && p[l] <= 'F') continue;
        break;
      }
      break;
  }
  return (l);
}

/*
 　機能：倍精度浮動小数点数をレジスタ2つに移動する
 戻り値：なし
*/
static void From_dbl(DBL *p, int reg) {
  rd[reg] = (p->c[7] << 24);
  rd[reg] |= (p->c[6] << 16);
  rd[reg] |= (p->c[5] << 8);
  rd[reg] |= p->c[4];
  rd[reg + 1] = (p->c[3] << 24);
  rd[reg + 1] |= (p->c[2] << 16);
  rd[reg + 1] |= (p->c[1] << 8);
  rd[reg + 1] |= p->c[0];
}

/*
 　機能：4バイト整数2つに入った倍精度浮動小数点数をエンディアン変換する
 戻り値：なし
*/
static void To_dbl(DBL *p, Long d0, Long d1) {
  p->c[0] = (d1 & 0xFF);
  p->c[1] = ((d1 >> 8) & 0xFF);
  p->c[2] = ((d1 >> 16) & 0xFF);
  p->c[3] = ((d1 >> 24) & 0xFF);
  p->c[4] = (d0 & 0xFF);
  p->c[5] = ((d0 >> 8) & 0xFF);
  p->c[6] = ((d0 >> 16) & 0xFF);
  p->c[7] = ((d0 >> 24) & 0xFF);
}

static void Pow(Long d0, Long d1, Long d2, Long d3) {
  DBL arg0, arg1;
  DBL ans;

  To_dbl(&arg0, d0, d1);
  To_dbl(&arg1, d2, d3);

  ans.dbl = pow(arg0.dbl, arg1.dbl);
  CCR_C_OFF();

  From_dbl(&ans, 0);
}