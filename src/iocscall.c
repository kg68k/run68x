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

#include "iocscall.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "host.h"
#include "iocscall.h"
#include "mem.h"
#include "operate.h"
#include "run68.h"

#if defined(USE_ICONV)
#include <iconv.h>
#endif

static Long Putc(UWord);
static Long Color(short);
static void Putmes(void);
static Long Dateasc(Long, Long);
static Long Timeasc(Long, Long);
static Long Dayasc(Long, Long);
static Long Intvcs(Long, Long);
static void Dmamove(Long, Long, Long, Long);

static int intToBcd(int n) { return ((n / 10) << 4) + n % 10; }

// IOCS _DATEBCD (0x50)
// 日付データのバイナリ→BCD変換。
ULong Datebcd(ULong b) {
  const int y = ((b >> 16) & 0xfff) - 1980;
  const int m = (b >> 8) & 0xff;
  const int d = b & 0xff;
  const int leapCount = y & 3;

  if (y < 0 || 99 < y || m < 1 || 12 < m || d < 1 || 31 < d) {
    return (ULong)-1;
  }
  if (m == 2) {
    // 2月は28日(閏年は29日)までしかない。
    const int lim = leapCount ? 28 : 29;
    if (lim < d) return (ULong)-1;
  } else if (d == 31) {
    // 4,6,9,11月は30日までしかない。
    if (m == 4 || m == 6 || m == 9 || m == 11) return (ULong)-1;
  }

  static const uint8_t wtable1[12] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
  static const uint8_t wtable1Leap[12] = {0, 3, 4, 0, 2, 5, 0, 3, 6, 1, 4, 6};
  static const uint8_t wtable2[7] = {2, 0, 5, 3, 1, 6, 4};
  static const uint8_t wtable3[4] = {0, 2, 3, 4};
  int w = (leapCount ? wtable1 : wtable1Leap)[m - 1];  // 1980年m月1日の曜日
  w += wtable2[(y / 4) % 7] + wtable3[leapCount] + (d - 1);  // y年m月d日の曜日

  return (leapCount << 28) | ((w % 7) << 24) | (intToBcd(y) << 16) |
         (intToBcd(m) << 8) | intToBcd(d);
}

// IOCS _DATESET (0x51)
// 日付の設定。
ULong Dateset(UNUSED ULong b) {
  // 下記理由によりホスト環境への設定は行わず、単に0を返す。
  // 1. 時計の変更はホスト環境への影響が大きすぎる。
  // 2. 変更してもネットワーク経由で自動的に修正されるため意味がない。
  return 0;
}

// IOCS _TIMEBCD (0x52)
// 時刻データのバイナリ→BCD変換。
ULong Timebcd(ULong b) {
  const int hh = (b >> 16) & 0xff;
  const int mm = (b >> 8) & 0xff;
  const int ss = b & 0xff;
  const int fmt = 1;  // 24時間計

  if (hh > 23 || mm > 59 || ss > 59) return (ULong)-1;

  return (fmt << 24) | (intToBcd(hh) << 16) | (intToBcd(mm) << 8) |
         intToBcd(ss);
}

// IOCS _TIMESET (0x53)
// 時刻の設定。
ULong Timeset(UNUSED ULong b) {
  // IOCS _DATESETと同じ理由によりホスト環境への設定は行わず、単に0を返す。
  return 0;
}

static struct tm toLocalTime(time_t timer) {
  struct tm result;

  if (timer == (time_t)-1 || HOST_TO_LOCALTIME(&timer, &result) == NULL) {
    result = (struct tm){
        .tm_sec = 0,
        .tm_min = 0,
        .tm_hour = 0,
        .tm_mday = 1,
        .tm_mon = 0,
        .tm_year = 1980 - 1900,
        .tm_wday = 2,
        .tm_yday = 0,
        .tm_isdst = 0,
    };
  }
  return result;
}

// IOCS _DATEGET (0x54)
// 日付データ(BCD)を返す。
ULong Dateget(TimeFunc timeFunc) {
  struct tm t = toLocalTime(timeFunc(NULL));

  // 1980年(または直前のxx80年)からの経過年数(0～99)
  const int dif = 1980 - 1900;
  int y = t.tm_year % 100;
  y = y + ((y < dif) ? 100 : 0) - dif;

  return (t.tm_wday << 24)                //
         | (intToBcd(y) << 16)            //
         | (intToBcd(t.tm_mon + 1) << 8)  //
         | intToBcd(t.tm_mday);
}

// IOCS _TIMEGET (0x54)
// 時刻データ(BCD)を返す。
ULong Timeget(TimeFunc timeFunc) {
  struct tm t = toLocalTime(timeFunc(NULL));
  const int fmt = 1;  // 24時間計

  return (fmt << 24)                    //
         | (intToBcd(t.tm_hour) << 16)  //
         | (intToBcd(t.tm_min) << 8)    //
         | intToBcd(t.tm_sec);
}

// IOCS _ONTIME (0x7f)
static RegPair IocsOntime(void) { return HOST_IOCS_ONTIME(); }

/*
 　機能：IOCSCALLを実行する
 戻り値： true = 実行終了
         false = 実行継続
*/
bool iocs_call() {
  int x, y;
  short save_s;

  UByte no = rd[0] & 0xff;

  if (settings.traceFunc) {
    printf("IOCS(%02X): PC=%06X\n", no, pc);
  }
  switch (no) {
    case 0x20: /* B_PUTC */
      rd[0] = Putc((rd[1] & 0xFFFF));
      break;
    case 0x21: /* B_PRINT */
    {
      char *p = GetStringSuper(ra[1]);
#if defined(USE_ICONV)
      // SJIS to UTF-8
      char utf8_buf[8192];
      iconv_t icd = iconv_open("UTF-8", "Shift_JIS");
      size_t inbytes = strlen(p);
      size_t outbytes = sizeof(utf8_buf) - 1;
      char *ptr_in = p;
      char *ptr_out = utf8_buf;
      memset(utf8_buf, 0x00, sizeof(utf8_buf));
      iconv(icd, &ptr_in, &inbytes, &ptr_out, &outbytes);
      iconv_close(icd);

      printf("%s", utf8_buf);
#else
      printf("%s", p);
#endif
      ra[1] += strlen(p);
      rd[0] = get_locate();
    } break;
    case 0x22: /* B_COLOR */
      rd[0] = Color((rd[1] & 0xFFFF));
      break;
    case 0x23: /* B_LOCATE */
      if (rd[1] != -1) {
        x = (rd[1] & 0xFFFF) + 1;
        y = (rd[2] & 0xFFFF) + 1;
        printf("%c[%d;%dH", 0x1B, y, x);
      }
      rd[0] = get_locate();
      break;
    case 0x24: /* B_DOWN_S */
      printf("%c[s\n%c[u%c[1B", 0x1B, 0x1B, 0x1B);
      break;
    case 0x25: /* B_UP_S */ /* (スクロール未サポート) */
      printf("%c[1A", 0x1B);
      break;
    case 0x2F: /* B_PUTMES */
      Putmes();
      break;
    case 0x50:  // _DATEBCD
      rd[0] = Datebcd(rd[1]);
      break;
    case 0x51:  // _DATESET
      rd[0] = Dateset(rd[1]);
      break;
    case 0x52:  // _TIMEBCD
      rd[0] = Timebcd(rd[1]);
      break;
    case 0x53:  // _TIMESET
      rd[0] = Timeset(rd[1]);
      break;
    case 0x54: /* DATEGET */
      rd[0] = Dateget(time);
      break;
    case 0x55: /* DATEBIN */
      rd[0] = Datebin(rd[1]);
      break;
    case 0x56: /* TIMEGET */
      rd[0] = Timeget(time);
      break;
    case 0x57: /* TIMEBIN */
      rd[0] = Timebin(rd[1]);
      break;
    case 0x5A: /* DATEASC */
      rd[0] = Dateasc(rd[1], ra[1]);
      break;
    case 0x5B: /* TIMEASC */
      rd[0] = Timeasc(rd[1], ra[1]);
      break;
    case 0x5C: /* DAYASC */
      rd[0] = Dayasc(rd[1], ra[1]);
      break;
    case 0x6C: /* VDISPST */
      save_s = SR_S_REF();
      SR_S_ON();
      if (ra[1] == 0) {
        mem_set(0x118, 0, S_LONG);
      } else {
        rd[0] = mem_get(0x118, S_LONG);
        if (rd[0] == 0) mem_set(0x118, ra[1], S_LONG);
      }
      if (save_s == 0) SR_S_OFF();
      break;
    case 0x6D: /* CRTCRAS */
      save_s = SR_S_REF();
      SR_S_ON();
      if (ra[1] == 0) {
        mem_set(0x138, 0, S_LONG);
      } else {
        rd[0] = mem_get(0x138, S_LONG);
        if (rd[0] == 0) mem_set(0x138, ra[1], S_LONG);
      }
      if (save_s == 0) SR_S_OFF();
      break;
    case 0x6E: /* HSYNCST */
      err68("水平同期割り込みを設定しようとしました");
    case 0x7F: /* ONTIME */
    {
      RegPair r = IocsOntime();
      rd[0] = r.r0;
      rd[1] = r.r1;
    } break;
    case 0x80: /* B_INTVCS */
      rd[0] = Intvcs(rd[1], ra[1]);
      break;
    case 0x81: /* B_SUPER */
      if (ra[1] == 0) {
        /* user -> super */
        if (SR_S_REF() != 0) {
          rd[0] = -1; /* エラー */
        } else {
          rd[0] = ra[7];
          SR_S_ON();
        }
      } else {
        /* super -> user */
        ra[7] = ra[1];
        rd[0] = 0;
        SR_S_OFF();
      }
      break;
    case 0x82: /* B_BPEEK */
      save_s = SR_S_REF();
      SR_S_ON();
      rd[0] = ((rd[0] & 0xFFFFFF00) | (mem_get(ra[1], S_BYTE) & 0xFF));
      if (save_s == 0) SR_S_OFF();
      ra[1] += 1;
      break;
    case 0x83: /* B_WPEEK */
      save_s = SR_S_REF();
      SR_S_ON();
      rd[0] = ((rd[0] & 0xFFFF0000) | (mem_get(ra[1], S_WORD) & 0xFFFF));
      if (save_s == 0) SR_S_OFF();
      ra[1] += 2;
      break;
    case 0x84: /* B_LPEEK */
      save_s = SR_S_REF();
      SR_S_ON();
      rd[0] = mem_get(ra[1], S_LONG);
      if (save_s == 0) SR_S_OFF();
      ra[1] += 4;
      break;
    case 0x8A: /* DMAMOVE */
      Dmamove(rd[1], rd[2], ra[1], ra[2]);
      break;
    case 0xAE: /* OS_CURON */
      printf("%c[>5l", 0x1B);
      break;
    case 0xAF: /* OS_CUROF */
      printf("%c[>5h", 0x1B);
      break;
    default:
      if (settings.traceFunc) {
        printf("IOCS(%02X): Unknown IOCS call. Ignored.\n", no);
      }
      break;
  }

  return false;
}

/*
 　機能：文字を表示する
 戻り値：カーソル位置
*/
static Long Putc(UWord code) {
  if (code == 0x1A) {
    printf("%c[0J", 0x1B); /* 最終行左端まで消去 */
  } else {
    if (code >= 0x0100) putchar(code >> 8);
    putchar(code & 0xff);
  }
  return (get_locate());
}

/*
 　機能：文字のカラー属性を指定する
 戻り値：変更前のカラーまたは現在のカラー
*/
static Long Color(short arg) {
  if (arg == -1) /* 現在のカラーを調べる(未サポート) */
    return (3);

  text_color(arg);

  return (3);
}

/*
 　機能：文字列を表示する
 戻り値：なし
*/
static void Putmes() {
  char temp[97];
  int x, y;
  int keta;
  int len;

  x = (rd[2] & 0xFFFF) + 1;
  y = (rd[3] & 0xFFFF) + 1;
  keta = (rd[4] & 0xFFFF) + 1;

  char *p = GetStringSuper(ra[1]);
  len = strlen(p);
  if (keta > 96) keta = 96;
  memcpy(temp, p, keta);
  temp[keta] = '\0';

  printf("%c[%d;%dH", 0x1B, y, x);
  text_color((rd[1] & 0xFF));
  printf("%s", temp);

  ra[1] += len;
}

/*
 　機能：BCD表現の日付データをバイナリ表現に直す
 戻り値：バイナリの日付データ
*/
ULong Datebin(Long bcd) {
  unsigned int youbi;
  unsigned int year;
  unsigned int month;
  unsigned int day;

  youbi = (bcd >> 24);
  year = ((bcd >> 20) & 0xF) * 10 + ((bcd >> 16) & 0xF) + 1980;
  month = ((bcd >> 12) & 0xF) * 10 + ((bcd >> 8) & 0xF);
  day = ((bcd >> 4) & 0xF) * 10 + (bcd & 0xF);

  return ((youbi << 28) | (year << 16) | (month << 8) | day);
}

/*
 　機能：BCD表現の時刻データをバイナリ表現に直す
 戻り値：バイナリの時刻データ
*/
ULong Timebin(Long bcd) {
  unsigned int hh;
  unsigned int mm;
  unsigned int ss;

  hh = ((bcd >> 20) & 0xF) * 10 + ((bcd >> 16) & 0xF);
  mm = ((bcd >> 12) & 0xF) * 10 + ((bcd >> 8) & 0xF);
  ss = ((bcd >> 4) & 0xF) * 10 + (bcd & 0xF);

  return ((hh << 16) | (mm << 8) | ss);
}

/*
 　機能：バイナリ表現の日付データを文字列に直す
 戻り値：-1のときエラー
*/
static Long Dateasc(Long data, Long adr) {
  unsigned int year = ((data >> 16) & 0xFFF);
  if (year < 1980 || year > 2079) return (-1);
  unsigned int month = ((data >> 8) & 0xFF);
  if (month < 1 || month > 12) return (-1);
  unsigned int day = (data & 0xFF);
  if (day < 1 || day > 31) return (-1);

  int sep = (data & (1 << 28)) ? '-' : '/';
  int yearLen = (data & (1 << 29)) ? 2 : 4;
  if (yearLen == 2) year %= 100;

  static const char fmt[] = "%0*d%c%02d%c%02d";
  char buf[16];
  snprintf(buf, sizeof(buf), fmt, yearLen, year, sep, month, sep, day);
  WriteStringSuper(adr, buf);
  ra[1] += strlen(buf);

  return 0;
}

/*
 　機能：バイナリ表現の時刻データを文字列に直す
 戻り値：-1のときエラー
*/
static Long Timeasc(Long data, Long adr) {
  unsigned int hh = ((data >> 16) & 0xFF);
  if (hh > 23) return (-1);
  unsigned int mm = ((data >> 8) & 0xFF);
  if (mm > 59) return (-1);
  unsigned int ss = (data & 0xFF);
  if (ss > 59) return (-1);

  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);
  WriteStringSuper(adr, buf);
  ra[1] += strlen(buf);

  return 0;
}

/*
 　機能：曜日番号から文字列を得る
 戻り値：なし
*/
static Long Dayasc(Long data, Long adr) {
  // 実行環境やソースコードのエンコーディングに左右されないように、
  // シフトJISの文字コードを直接埋め込んでいる。
  static const char days[8][4] = {
      {"\x93\xfa"},  // 0: 日
      {"\x8c\x8e"},  // 1: 月
      {"\x89\xce"},  // 2: 火
      {"\x90\x85"},  // 3: 水
      {"\x96\xd8"},  // 4: 木
      {"\x8b\xe0"},  // 5: 金
      {"\x93\x79"},  // 6: 土
      {"\x81\x48"},  // 7: ？
  };

  WriteStringSuper(adr, days[data & 7]);
  ra[1] += 2;
  return 0;
}

/*
 　機能：ベクタ・テーブルを書き換える
 戻り値：設定前の処理アドレス
*/
static Long Intvcs(Long no, Long adr) {
  Long adr2;
  Long mae = 0;
  short save_s;

  no &= 0xFFFF;
  adr2 = no * 4;
  save_s = SR_S_REF();
  SR_S_ON();
  mae = mem_get(adr2, S_LONG);
  mem_set(adr2, adr, S_LONG);
  if (save_s == 0) SR_S_OFF();

  return (mae);
}

/*
 　機能：DMA転送をする
 戻り値：設定前の処理アドレス
*/
static void Dmamove(Long md, Long size, Long adr1, Long adr2) {
  if ((md & 0x80) != 0) {
    /* adr1 -> adr2転送にする */
    Long tmp = adr1;
    adr1 = adr2;
    adr2 = tmp;
  }

  /* adr1,adr2共にインクリメントモードでない場合は未サポート */
  if ((md & 0x0F) != 5) return;

  Span r, w;
  if (!GetReadableMemoryRangeSuper(adr1, size, &r)) return;
  if (!GetWritableMemoryRangeSuper(adr2, r.length, &w)) return;
  memcpy(w.bufptr, r.bufptr, w.length);
}

/* $Id: iocscall.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:07  masamic
 * First imported source code and docs
 *
 * Revision 1.3  1999/12/07  12:42:59  yfujii
 * *** empty log message ***
 *
 * Revision 1.3  1999/10/25  03:24:58  yfujii
 * Trace output is now controlled with command option.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
