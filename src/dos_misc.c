// run68x - Human68k CUI Emulator based on run68
// Copyright (C) 2025 TcbnErik
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

#include "dos_misc.h"

#include <string.h>
#include <time.h>

#include "iocscall.h"
#include "mem.h"
#include "run68.h"

// DOS _GETTIM2 (0xff27) 内部処理
static Long Gettim2(TimeFunc timeFunc) { return Timebin(Timeget(timeFunc)); }

// DOS _GETTIM2 (0xff27)
Long DosGettim2(void) { return Gettim2(time); }

// DOS _SETTIM2 (0xff28) 内部処理
static Long Settim2(ULong binTime) {
  const ULong bcd = Timebcd(binTime);
  if (bcd == (ULong)-1) return DOSE_ILGFNC;

  // 現状の仕様ではIOCS _TIMESETがホスト環境への設定を行わないため、
  // DOS _SETTIM2でも設定は行われない。
  Timeset(bcd);
  return DOSE_SUCCESS;
}

// DOS _SETTIM2 (0xff28)
Long DosSettim2(ULong param) { return Settim2(ReadParamULong(&param)); }

// DOS _GETDATE (0xff2a) 内部処理
static Long Getdate(TimeFunc timeFunc) {
  const ULong date = Datebin(Dateget(timeFunc));

  const int w = (date >> 28) & 7;
  const int y = ((date >> 16) & 0xfff) - 1980;
  const int m = (date >> 8) & 0xf;
  const int d = date & 0x1f;
  return (w << 16) | (y << 9) | (m << 5) | d;
}

// DOS _GETDATE (0xff2a)
Long DosGetdate(void) { return Getdate(time); }

// DOS _SETDATE (0xff2b) 内部処理
static Long Setdate(UWord date) {
  const int y = (date >> 9) + 1980;
  const int m = (date >> 5) & 0xf;
  const int d = date & 0x1f;
  const ULong bcd = Datebcd((y << 16) | (m << 8) | d);
  if (bcd == (ULong)-1) return DOSE_ILGFNC;

  // 現状の仕様ではIOCS _DATESETがホスト環境への設定を行わないため、
  // DOS _SETDATEでも設定は行われない。
  Dateset(bcd);
  return DOSE_SUCCESS;
}

// DOS _SETDATE (0xff2b)
Long DosSetdate(ULong param) { return Setdate(ReadParamUWord(&param)); }

// DOS _GETTIME (0xff2c) 内部処理
static Long Gettime(TimeFunc timeFunc) {
  const ULong t = Gettim2(timeFunc);

  const int h = t >> 16;
  const int m = (t >> 8) & 0x3f;
  const int s = (t & 0x3f) / 2;
  return (h << 11) | (m << 5) | s;
}

// DOS _GETTIME (0xff2c)
Long DosGettime(void) { return Gettime(time); }

// DOS _SETTIME (0xff2d) 内部処理
static Long Settime(UWord binTime) {
  const int h = binTime >> 11;
  const int m = (binTime >> 5) & 0x3f;
  const int s = (binTime & 0x1f) * 2;

  const ULong bcd = Timebcd((h << 16) | (m << 8) | s);
  if (bcd == (ULong)-1) return DOSE_ILGFNC;

  // 現状の仕様ではIOCS _TIMESETがホスト環境への設定を行わないため、
  // DOS _SETTIMEでも設定は行われない。
  Timeset(bcd);
  return DOSE_SUCCESS;
}

// DOS _SETTIME (0xff2d)
Long DosSettime(ULong param) { return Settime(ReadParamUWord(&param)); }

// DOS _VERNUM (0xff30)
Long DosVernum(void) {
  const ULong id = 0x3638;   // '68'
  const ULong ver = 0x0302;  // v3.02

  return (id << 16) | ver;
}

// 環境変数領域から環境変数を検索する。
const char* Getenv(const char* name, ULong env) {
  if (env == 0) {
    env = ReadULongSuper(psp[nest_cnt] + PSP_ENV_PTR);
  }
  if (env == (ULong)-1) return NULL;

  // 環境変数領域の先頭4バイトは領域サイズで、その次から文字列が並ぶ。
  ULong kv = env + 4;
  const size_t len = strlen(name);

  for (;;) {
    char* p = GetStringSuper(kv);
    if (*p == '\0') break;

    if (memcmp(p, name, len) == 0 && p[len] == '=') {
      return p + len + strlen("=");
    }
    kv += strlen(p) + 1;
  }
  return NULL;
}

// DOS _GETENV (0xff53, 0xff53)
Long DosGetenv(ULong param) {
  ULong name = ReadParamULong(&param);
  ULong env = ReadParamULong(&param);
  ULong buf = ReadParamULong(&param);

  const char* s = Getenv(GetStringSuper(name), env);
  if (!s) return DOSE_ILGFNC;

  WriteStringSuper(buf, s);
  return DOSE_SUCCESS;
}

// DOS _BUS_ERR (0xfff7)
Long DosBusErr(ULong param) {
  ULong s_adr = ReadParamULong(&param);
  ULong d_adr = ReadParamULong(&param);
  UWord size = ReadParamUWord(&param);

  if (size == 1) {
    // バイトアクセス
  } else if (size == 2 || size == 4) {
    // ワード、ロングワードアクセス
    if ((s_adr & 1) != 0 || (d_adr & 1) != 0)
      return DOSE_ILGFNC;  // 奇数アドレス
  } else {
    return DOSE_ILGFNC;  // サイズが不正
  }

  Span r = GetReadableMemorySuper(s_adr, size);
  if (!r.bufptr) return 2;  // 読み込み時にバスエラー発生
  Span w = GetWritableMemorySuper(d_adr, size);
  if (!w.bufptr) return 1;  // 書き込み時にバスエラー発生

  if (size == 1)
    PokeB(w.bufptr, PeekB(r.bufptr));
  else if (size == 2)
    PokeW(w.bufptr, PeekW(r.bufptr));
  else
    PokeL(w.bufptr, PeekL(r.bufptr));

  return 0;
}
