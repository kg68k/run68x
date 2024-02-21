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

#include "fefunc.h"

#include <ctype.h>
#include <string.h>

#include "human68k.h"
#include "m68k.h"
#include "mem.h"
#include "run68.h"

#ifdef __BIG_ENDIAN__
#define DBL_U_HIGH 0  // dbl.l[0]
#define DBL_U_LOW 1   // dbl.l[1]
#define DBL_C_SIGN 0  // dbl.c[0]
#else
#define DBL_U_HIGH 1  // dbl.l[1]
#define DBL_U_LOW 0   // dbl.l[0]
#define DBL_C_SIGN 7  // dbl.c[7]
#endif

typedef enum {
  FPTYPE_ZERO,
  FPTYPE_NORMALIZED,
  FPTYPE_DENORMALIZED,
  FPTYPE_INFINITE,
  FPTYPE_NAN,
} FPDataType;

static DBL ToDbl(ULong d0, ULong d1) {
  DBL dbl;
  dbl.l[DBL_U_LOW] = d1;
  dbl.l[DBL_U_HIGH] = d0;
  return dbl;
}

static int AbsDbl(DBL *dbl) {
  int sign = dbl->c[DBL_C_SIGN] >> 7;
  dbl->c[DBL_C_SIGN] &= 0x7f;
  return sign;
}

static FPDataType GetFPDataType(DBL dbl) {
  ULong exp = dbl.l[DBL_U_HIGH] & 0x7ff00000;
  ULong mant0 = dbl.l[DBL_U_HIGH] & 0x000fffff;
  ULong mant1 = dbl.l[DBL_U_LOW];

  if (exp == 0) {
    return (mant0 == 0 && mant1 == 0) ? FPTYPE_ZERO : FPTYPE_DENORMALIZED;
  }
  if (exp == 0x7ff00000) {
    return (mant0 == 0 && mant1 == 0) ? FPTYPE_INFINITE : FPTYPE_NAN;
  }
  return FPTYPE_NORMALIZED;
}

static void SetCcr(int f) { sr = (sr & SR_MASK) | f; }

// FPACK __STOH (0xfe12)
ULong FefuncStoh(Long *pA0) {
  ULong result = 0;
  bool nodigit = true;
  ULong c;

  while (isxdigit(c = ReadUByteSuper(*pA0))) {
    if (result > 0x0fffffff) {
      SetCcr(CCR_V | CCR_C);
      return c;
    }
    ULong x = isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
    result = (result << 4) + x;

    nodigit = false;
    *pA0 += 1;
  }
  if (nodigit) {
    SetCcr(CCR_N | CCR_C);
    return c;
  }

  SetCcr(0);
  return result;
}

static int fconvert(double d, ULong adr, int ndigit, size_t maxlen) {
  char fmt[1024];

  int precision = (d < 1.0) ? 307 : ndigit;
  int n = snprintf(fmt, sizeof(fmt), "%.*f", precision, d);
  if (n < 0) {
    WriteStringSuper(adr, "");
    return 0;
  }

  // "0." + 小数部(1.0未満)
  if (fmt[0] == '0' && fmt[1] == '.') {
    char *decpart = fmt + 2;
    char *p = decpart;
    while (*p == '0') p += 1;  // "0."と小数部先頭の"0"を取り除く
    char *eos = decpart + ndigit;
    *eos = '\0';
    WriteStringSuper(adr, (p < eos) ? p : eos);
    return decpart - p;
  }

  char *point = strchr(fmt, '.');
  // 整数部 + "." + 小数部
  if (point != NULL) {
    point[ndigit + 1] = '\0';
    memmove(point, point + 1, strlen(point + 1) + 1);  // 小数点"."を取り除く
    fmt[FEFUNC_FCVT_INT_MAXLEN] = '\0';
    WriteStringSuper(adr, fmt);
    return point - fmt;
  }

  // 整数部のみ
  {
    int decpt = strlen(fmt);
    fmt[FEFUNC_FCVT_INT_MAXLEN] = '\0';
    WriteStringSuper(adr, fmt);
    return decpt;
  }
}

// FPACK __FCVT (0xfe25)
void FefuncFcvt(Long *pD0, Long *pD1, ULong ndigit, ULong adr) {
  const size_t maxlen = 255;

  DBL dbl = ToDbl(*pD0, *pD1);
  *pD1 = AbsDbl(&dbl);

  switch (GetFPDataType(dbl)) {
    case FPTYPE_NORMALIZED:
    case FPTYPE_DENORMALIZED:
      *pD0 = (ULong)fconvert(dbl.dbl, adr, ndigit & 0xff, maxlen);
      break;

    case FPTYPE_ZERO: {
      char buf[256];
      size_t len = ndigit & 0xff;
      memset(buf, '0', len);
      buf[len] = '\0';
      WriteStringSuper(adr, buf);
      *pD0 = 0;
    } break;

    case FPTYPE_INFINITE:
      WriteStringSuper(adr, "#INF");
      *pD0 = 4;
      break;

    case FPTYPE_NAN:
      WriteStringSuper(adr, "#NAN");
      *pD0 = 4;
      break;
  }
}
