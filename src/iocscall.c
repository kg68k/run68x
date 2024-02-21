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
#include <string.h>

#include "host.h"
#include "mem.h"
#include "run68.h"

#if defined(USE_ICONV)
#include <iconv.h>
#endif

#include <time.h>

static Long Putc(UWord);
static Long Color(short);
static void Putmes(void);
static Long Dateget(void);
static Long Timeget(void);
static Long Datebin(Long);
static Long Timebin(Long);
static Long Dateasc(Long, Long);
static Long Timeasc(Long, Long);
static void Dayasc(Long, Long);
static Long Intvcs(Long, Long);
static void Dmamove(Long, Long, Long, Long);

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

  if (func_trace_f) {
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
    case 0x54: /* DATEGET */
      rd[0] = Dateget();
      break;
    case 0x55: /* DATEBIN */
      rd[0] = Datebin(rd[1]);
      break;
    case 0x56: /* TIMEGET */
      rd[0] = Timeget();
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
      Dayasc(rd[1], ra[1]);
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
      if (func_trace_f) {
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
 　機能：日付を得る
 戻り値：BCDの日付データ
*/
static Long Dateget() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  return (t->tm_wday << 24)                  //
         | (((t->tm_year - 80) / 10) << 20)  //
         | (((t->tm_year - 80) % 10) << 16)  //
         | ((t->tm_mon / 10) << 12)          //
         | ((t->tm_mon % 10) << 8)           //
         | ((t->tm_mday / 10) << 4)          //
         | (t->tm_mday % 10);
}

/*
 　機能：時刻を得る
 戻り値：BCDの時刻データ
*/
static Long Timeget() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  return ((t->tm_hour / 10) << 20)    //
         | ((t->tm_hour % 10) << 16)  //
         | ((t->tm_min / 10) << 12)   //
         | ((t->tm_min % 10) << 8)    //
         | ((t->tm_sec / 10) << 4)    //
         | (t->tm_sec % 10);
}

/*
 　機能：BCD表現の日付データをバイナリ表現に直す
 戻り値：バイナリの日付データ
*/
static Long Datebin(Long bcd) {
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
static Long Timebin(Long bcd) {
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
static void Dayasc(Long data, Long adr) {
  char data_ptr[16];

  switch (data) {
    case 0:
      strcpy(data_ptr, "日");
      break;
    case 1:
      strcpy(data_ptr, "月");
      break;
    case 2:
      strcpy(data_ptr, "火");
      break;
    case 3:
      strcpy(data_ptr, "水");
      break;
    case 4:
      strcpy(data_ptr, "木");
      break;
    case 5:
      strcpy(data_ptr, "金");
      break;
    case 6:
      strcpy(data_ptr, "土");
      break;
    default:
      return;
  }
  WriteStringSuper(adr, data_ptr);
  ra[1] += strlen(data_ptr);
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

  MemoryRange r, w;
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
