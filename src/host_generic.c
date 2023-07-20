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

#ifndef USE_ICONV
#error "USE_ICONV muse be defined."
#endif

#include <iconv.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "human68k.h"
#include "run68.h"

#define ROOT_SLASH_LEN 1  // "/"

static FILE *fileno_to_fp(int fileno) {
  if (fileno == HUMAN68K_STDIN) return stdin;
  if (fileno == HUMAN68K_STDOUT) return stdout;
  if (fileno == HUMAN68K_STDERR) return stderr;
  return NULL;
}

// FINFO構造体の環境依存メンバーを初期化する
void InitFileInfo_generic(FILEINFO *finfop, int fileno) {
  finfop->host.fp = fileno_to_fp(fileno);
}

// ファイルを閉じる
bool CloseFile_generic(FILEINFO *finfop) {
  FILE *fp = finfop->host.fp;
  if (fp == NULL) return false;

  finfop->host.fp = NULL;
  return fclose(fp) == EOF ? false : true;
}

static void not_implemented(const char *name) {
  fprintf(stderr, "run68: %s()は未実装です。\n", name);
}

// DOS _MKDIR (0xff39) (未実装)
Long DosMkdir_generic(Long name) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}

// DOS _RMDIR (0xff3a) (未実装)
Long DosRmdir_generic(Long name) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}

// DOS _CHDIR (0xff3b) (未実装)
Long DosChdir_generic(Long name) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}

// UTF-8文字列からShift_JIS文字列への変換
static size_t utf8_to_sjis(char *inbuf, char *outbuf, size_t outbuf_size) {
  iconv_t icd = iconv_open("Shift_JIS", "UTF-8");
  size_t inbytes = strlen(inbuf);
  size_t outbytes = outbuf_size - 1;
  size_t result = iconv(icd, &inbuf, &inbytes, &outbuf, &outbytes);
  iconv_close(icd);
  *outbuf = '\0';
  return result;
}

// Shift_JIS文字列中のスラッシュをバックスラッシュに書き換える
static void to_backslash(char *buf) {
  for (; *buf; buf += 1) {
    if (*buf == '/') {
      *buf = '\\';
    }
  }
}

// DOS _CURDIR (0xff47)
Long DosCurdir_generic(short drv, char *buf_ptr) {
  char buf[PATH_MAX];
  const char *p = getcwd(buf, sizeof(buf));
  if (p == NULL) {
    // Human68kのDOS _CURDIRはエラーコードとして-15しか返さないので
    // getdcwd()が失敗する理由は考慮しなくてよい。
    return DOSE_ILGDRV;
  }
  if (utf8_to_sjis(buf + ROOT_SLASH_LEN, buf_ptr, HUMAN68K_PATH_MAX) ==
      (size_t)-1) {
    return DOSE_ILGDRV;
  }
  to_backslash(buf_ptr);
  return 0;
}

// DOS _FILEDATE (0xff87) (未実装)
Long DosFiledate_generic(short hdl, Long dt) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}
