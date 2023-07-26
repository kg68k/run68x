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

// TODO: UTF-8 から Shift_JIS への変換

#include "hupair.h"

#include <string.h>

#include "dos_memory.h"
#include "host_generic.h"
#include "host_win32.h"
#include "run68.h"

static const char hupairMark[] = "#HUPAIR";

typedef struct {
  ULong len;
  char ch;  // 0, '"', '\''
} Quoting;

// '"' または '\'' が見つかった場合
static Quoting quotFound(const char* top, const char* s, char c) {
  char quot = c ^ '"' ^ '\'';
  char x;

  // 文字列末尾またはクオートと同じ文字まで一度にクオート処理できる
  while ((x = *s++) != '\0') {
    if (x == quot) break;
  }
  s -= 1;
  return (Quoting){s - top, quot};
}

// スペースが見つかった場合
static Quoting spaceFound(const char* top, const char* s) {
  char c;

  // スペースより後ろにあるクオート文字を探す
  while ((c = *s++) != '\0') {
    if (c == '"' || c == '\'') return quotFound(top, s, c);
  }
  s -= 1;

  // クオート文字がなければ全体を '"' でクオートする('\'' でもよい)
  return (Quoting){s - top, '"'};
}

// クオート文字とそのクオートで処理できるバイト数を調べる
static Quoting getQuotingType(const char* top) {
  const char* s = top;
  char c;

  while ((c = *s++) != '\0') {
    if (c == '"' || c == '\'') {
      return quotFound(top, s, c);
    }
    if (c == ' ') {
      return spaceFound(top, s);
    }
  }
  s -= 1;

  // クオートが必要な文字はなかった
  return (Quoting){s - top, 0};
}

static inline bool ensureCapacity(ULong* size, ULong required) {
  if (*size < required) return false;
  *size -= required;
  return true;
}

#ifdef USE_ICONV
typedef struct {
  char* buf;
  size_t size;
} IconvBufPtr;

static inline bool alloc_iconv_buf(IconvBufPtr* ibp, size_t newSize) {
  if (newSize > ibp->size) {
    if (newSize < 4096) newSize = 4096;
    free(ibp->buf);
    *ibp = (IconvBufPtr){malloc(newSize), newSize};
  }
  return (ibp->buf == NULL) ? false : true;
}

static inline void free_iconv_buf(IconvBufPtr* ibp) {
  free(ibp->buf);
  *ibp = (IconvBufPtr){NULL, 0};
}
#endif

static ULong encodeHupair(int argc, char* argv[], const char* argv0, ULong adr,
                          ULong size) {
  char* p = prog_ptr + adr;
  const char* const buffer_top = p;

  // コマンドラインの手前にHUPAIR識別子
  size_t len = sizeof(hupairMark);  // 末尾のNUL文字も含んだ8バイト
  if (!ensureCapacity(&size, len)) return 0;
  memcpy(p, hupairMark, len);
  p += len;

  // コマンドライン文字列の長さ(現時点では不明)
  if (!ensureCapacity(&size, sizeof(UByte))) return 0;
  UByte* const cmdline_len_ptr = (UByte*)p;
  *p++ = 0;

  // コマンドライン文字列の先頭
  const char* const cmdline_top = p;

#ifdef USE_ICONV
  IconvBufPtr iconvBufPtr = {NULL, 0};
#endif

  for (int index = 0; index < argc; index += 1) {
    char* s = argv[index];
#ifdef USE_ICONV
    if (!alloc_iconv_buf(&iconvBufPtr, strlen(s) + 1)) return 0;
    if (!HOST_CONVERT_TO_SJIS(s, iconvBufPtr.buf, iconvBufPtr.size)) return 0;
    s = iconvBufPtr.buf;
#endif

    if (index > 0) {
      if (!ensureCapacity(&size, sizeof(char))) return 0;
      *p++ = ' ';
    }
    // 空文字列は "" とする
    if (*s == '\0') {
      if (!ensureCapacity(&size, 2)) return 0;
      *p++ = '"';
      *p++ = '"';
      continue;
    }

    do {
      Quoting q = getQuotingType(s);
      if (!ensureCapacity(&size, q.len + (q.ch ? 2 : 0))) return 0;

      if (q.ch) *p++ = q.ch;
      memcpy(p, s, q.len);
      p += q.len;
      if (q.ch) *p++ = q.ch;

      s += q.len;
    } while (*s);
  }

  // コマンドライン文字列の長さが決定したので埋め込む
  len = p - cmdline_top;
  *cmdline_len_ptr = (len > 255) ? 255 : len;

  if (!ensureCapacity(&size, sizeof(char))) return 0;
  *p++ = '\0';

  // argv0 を格納する
  len = strlen(argv0) + sizeof(char);
  if (!ensureCapacity(&size, len)) return 0;
  strcpy(p, argv0);  // 末尾のNUL文字がHUPAIRコマンドラインの最後のデータ
  p += len;

#ifdef USE_ICONV
  free_iconv_buf(&iconvBufPtr);
#endif

  // 実際に使用したバイト数を返す
  return (ULong)(p - buffer_top);
}

ULong EncodeHupair(int argc, char* argv[], const char* argv0, ULong parent) {
  ULong size = Malloc(MALLOC_FROM_LOWER, (ULong)-1, parent) & 0x00ffffff;
  ULong adr = Malloc(MALLOC_FROM_LOWER, size, parent);
  if ((Long)adr < 0) return 0;

  ULong consumed = encodeHupair(argc, argv, argv0, adr, size);
  if (consumed == 0) {
    Mfree(adr);
    return 0;
  }
  Setblock(adr, consumed);

  // コマンドライン先頭(文字列長のアドレス)を返す
  return adr + (ULong)sizeof(hupairMark);
}
