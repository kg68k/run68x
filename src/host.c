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

#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef USE_ICONV
#include <iconv.h>
#endif

#include "host.h"
#include "human68k.h"
#include "run68.h"

#define ROOT_SLASH_LEN 1  // "/"
#define DEFAULT_DRV_CLN "A:"

#ifdef HOST_CONVERT_TO_SJIS_GENERIC_ICONV
// UTF-8文字列からShift_JIS文字列への変換
bool Utf8ToSjis_generic_iconv(char *inbuf, char *outbuf, size_t outbuf_size) {
  iconv_t icd = iconv_open("Shift_JIS", "UTF-8");
  size_t inbytes = strlen(inbuf);
  size_t outbytes = outbuf_size - 1;
  size_t len = iconv(icd, &inbuf, &inbytes, &outbuf, &outbytes);
  iconv_close(icd);
  *outbuf = '\0';
  return len != (size_t)-1;
}
#endif

#ifdef HOST_CONVERT_TO_SJIS_GENERIC
// Shift_JIS文字列からShift_JIS文字列への無変換コピー
bool SjisToSjis_generic(char *inbuf, char *outbuf, size_t outbuf_size) {
  size_t len = strlen(inbuf);
  if (len >= outbuf_size) return false;
  strcpy(outbuf, inbuf);
  return true;
}
#endif

// Shift_JIS文字列中のスラッシュをバックスラッシュに書き換える
static void to_backslash(char *buf) {
  for (; *buf; buf += 1) {
    if (*buf == '/') {
      *buf = '\\';
    }
  }
}

#ifdef HOST_CANONICAL_PATHNAME_GENERIC
static bool canonical_pathname(char *fullpath, Human68kPathName *hpn) {
  char buf[HUMAN68K_PATH_MAX + 1];

  if (!HOST_CONVERT_TO_SJIS(fullpath, buf, sizeof(buf))) return false;

  char *lastSlash = strrchr(buf, '/');
  if (lastSlash == NULL) return false;

  char *name = lastSlash + 1;
  size_t pathLen = name - buf;
  if (pathLen > HUMAN68K_DIR_MAX) return false;

  size_t nameLen = strlen(name);  // 拡張子を含む長さなので後で差し引く
  char *ext = strrchr(name, '.');
  if (ext == NULL) ext = name + nameLen;

  size_t extLen = strlen(ext);
  nameLen -= extLen;
  if (extLen > HUMAN68K_EXT_MAX) {
    nameLen += extLen;
    extLen = 0;
  }
  if (nameLen > HUMAN68K_NAME_MAX) return false;

  const char *prefix = DEFAULT_DRV_CLN;
  const size_t prefixLen = strlen(DEFAULT_DRV_CLN);
  strcpy(hpn->path, prefix);
  strncpy(hpn->path + prefixLen, buf, pathLen);
  hpn->path[prefixLen + pathLen] = '\0';

  to_backslash(hpn->path);
  strcpy(hpn->name, name);

  hpn->nameLen = nameLen;
  hpn->extLen = extLen;

  return true;
}

// パス名の正規化
bool CanonicalPathName_generic(const char *path, Human68kPathName *hpn) {
  char *buf = realpath(path, NULL);
  if (buf == NULL) return false;

  bool result = canonical_pathname(buf, hpn);
  free(buf);
  return result;
}
#endif

#ifdef HOST_ADD_LAST_SEPARATOR_GENERIC
// パス名の末尾にパスデリミタを追加する
void AddLastSeparator_generic(char *path) {
  size_t len = strlen(path);
  if (len == 0 || path[len - 1] != '/') strcpy(path + len, "/");
}
#endif

#ifdef HOST_PATH_IS_FILE_SPEC_GENERIC
// 文字列にパス区切り文字が含まれないか(ファイル名だけか)を調べる
//   true -> ファイル名のみ
//   false -> "/" が含まれる
bool PathIsFileSpec_generic(const char *path) {
  return strchr(path, '/') == NULL;
}
#endif

#ifdef HOST_INIT_FILEINFO_GENERIC
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
#endif

#ifdef HOST_CLOSE_FILE_GENERIC
// ファイルを閉じる
bool CloseFile_generic(FILEINFO *finfop) {
  FILE *fp = finfop->host.fp;
  if (fp == NULL) return false;

  finfop->host.fp = NULL;
  return fclose(fp) == EOF ? false : true;
}
#endif

static void not_implemented(const char *name) {
  fprintf(stderr, "run68: %s()は未実装です。\n", name);
}

#ifdef HOST_DOS_MKDIR_GENERIC
// DOS _MKDIR (0xff39) (未実装)
Long DosMkdir_generic(Long name) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}
#endif

#ifdef HOST_DOS_RMDIR_GENERIC
// DOS _RMDIR (0xff3a) (未実装)
Long DosRmdir_generic(Long name) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}
#endif

#ifdef HOST_DOS_CHDIR_GENERIC
// DOS _CHDIR (0xff3b) (未実装)
Long DosChdir_generic(Long name) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}
#endif

#ifdef HOST_DOS_CURDIR_GENERIC
// DOS _CURDIR (0xff47)
Long DosCurdir_generic(short drv, char *buf_ptr) {
  char buf[PATH_MAX];
  const char *p = getcwd(buf, sizeof(buf));
  if (p == NULL) {
    // Human68kのDOS _CURDIRはエラーコードとして-15しか返さないので
    // getdcwd()が失敗する理由は考慮しなくてよい。
    return DOSE_ILGDRV;
  }
  if (!HOST_CONVERT_TO_SJIS(buf + ROOT_SLASH_LEN, buf_ptr,
                            (HUMAN68K_DIR_MAX - 1) + 1)) {
    return DOSE_ILGDRV;
  }
  to_backslash(buf_ptr);
  return 0;
}
#endif

#ifdef HOST_DOS_FILEDATE_GENERIC
// DOS _FILEDATE (0xff57, 0xff87) (未実装)
Long DosFiledate_generic(UWord fileno, ULong dt) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}
#endif
