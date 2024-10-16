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
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "host.h"
#include "human68k.h"
#include "mem.h"
#include "operate.h"
#include "run68.h"

static UByte xhead[XHEAD_SIZE];

static Long xhead_getl(int);

#ifdef _WIN32
#define PATH_DELIMITER ';'
#else
#define PATH_DELIMITER ':'
#endif

static char *GetAPath(const char **path_p, size_t bufSize, char *buf) {
  unsigned int i;
  *buf = '\0';

  if (path_p == NULL || *path_p == NULL) return NULL;

  while ((*path_p)[0] != '\0') {
    const size_t len = strlen(*path_p);
    for (i = 0; i < len && (*path_p)[i] != PATH_DELIMITER; i++) {
      /* 2バイトコードのスキップ */
      ;
    }
    int skip = i + (((*path_p)[i] == '\0') ? 0 : 1);
    if (i >= (bufSize - 1)) {  // パスデリミタを追加する分を差し引いておく
      *path_p += skip;
      continue;
    }

    strncpy(buf, *path_p, i);
    buf[i] = '\0';
    HOST_ADD_LAST_SEPARATOR(buf);

    *path_p += skip;
    return buf;
  }
  return NULL;
}

// prog_open()、prog_read()でエラー処理時のコールバックに
// NULLが指定された場合のダミー関数
static void onErrorDummy(const char *message) {
  //
}

/*
  機能：
    実行ファイルをオープンする。環境変数のPATHから取得したパスを
    順番に探索して最初に見付かったファイルをオープンする。
    最初にカレントディレクトリを検索する。
  引数：
    char *fname     -- ファイル名文字列
    bool print_error -- trueの時メッセージを標準エラー出力に出力
  戻り値：
    NULL = オープンできない
    !NULL = 実行ファイルのファイルポインタ
*/
FILE *prog_open(char *fname, ULong envptr, void (*err)(const char *)) {
  char dir[MAX_PATH], fullname[MAX_PATH] = {0}, cwd[MAX_PATH];
  FILE *fp = 0;
  char *exp = strrchr(fname, '.');
  void (*onError)(const char *) = err ? err : onErrorDummy;

  if (!HOST_PATH_IS_FILE_SPEC(fname)) {
    // パス区切り文字が含まれる場合は拡張子補完のみ行い、パス検索は行わない
    strcpy(fullname, fname);
    if ((fp = fopen(fullname, "rb")) != NULL) goto EndOfFunc;
    // ここから追加(by Yokko氏)
    strcat(fullname, ".r");
    if ((fp = fopen(fullname, "rb")) != NULL) goto EndOfFunc;
    strcpy(fullname, fname);
    strcat(fullname, ".x");
    if ((fp = fopen(fullname, "rb")) != NULL) goto EndOfFunc;
    // ここまで追加(by Yokko氏)
    goto ErrorRet;
  }
  if (exp != NULL && !_stricmp(exp, ".x") && !_stricmp(exp, ".r"))
    goto ErrorRet; /* 拡張子が違う */
#ifdef _WIN32
  GetCurrentDirectory(sizeof(cwd) - 1, cwd);
#else
  if (getcwd(cwd, sizeof(cwd) - 1) == NULL) {
    onError("カレントディレクトリのパス名が長すぎます\n");
    return NULL;
  }
#endif
  HOST_ADD_LAST_SEPARATOR(cwd);

  /* PATH環境変数を取得する */
#ifdef _WIN32
  const char *env_p = Getenv("path", envptr);
#else
  // 現在の実装ではHuman68kの環境変数ではなく、ホスト(Linux等)の環境変数を読み込んでいる。
  const char *env_p = getenv("PATH");
#endif
  for (strcpy(dir, cwd); strlen(dir) != 0; GetAPath(&env_p, sizeof(dir), dir)) {
    size_t len = strlen(dir) + strlen("/") + strlen(fname) + strlen(".x");
    if (len >= 89) {
      // printf("too long path: %s\n", dir);
      continue;
    }

    if (exp != NULL) {
      strcpy(fullname, dir);
      strcat(fullname, fname);
      if ((fp = fopen(fullname, "rb")) != NULL) goto EndOfFunc;
    } else {
      strcpy(fullname, dir);
      strcat(fullname, fname);
      strcat(fullname, ".r");
      if ((fp = fopen(fullname, "rb")) != NULL) goto EndOfFunc;
      strcpy(fullname, dir);
      strcat(fullname, fname);
      strcat(fullname, ".x");
      if ((fp = fopen(fullname, "rb")) != NULL) goto EndOfFunc;
    }
  }
EndOfFunc:
  strcpy(fname, fullname);
  return fp;
ErrorRet:
  onError("ファイルがオープンできません\n");
  return NULL;
}

/*
 　機能：Xファイルをリロケートする
 戻り値： true = 正常終了
 　　　　false = 異常終了
*/
static bool xrelocate(Long reloc_adr, Long reloc_size, Long read_top) {
  Long prog_adr;

  prog_adr = read_top;
  while (reloc_size > 0) {
    ULong disp = mem_get(read_top + reloc_adr, S_WORD);
    reloc_size -= 2;
    reloc_adr += 2;

    if (disp == 1) {
      disp = mem_get(read_top + reloc_adr, S_LONG);
      reloc_size -= 4;
      reloc_adr += 4;
    }
    if (disp & 1) {
      prog_adr += (disp & ~1);
      mem_set(prog_adr, mem_get(prog_adr, S_WORD) + read_top, S_WORD);
    } else {
      prog_adr += disp;
      mem_set(prog_adr, mem_get(prog_adr, S_LONG) + read_top, S_LONG);
    }
  }

  return true;
}

/*
 　機能：Xファイルをコンバートする
 戻り値： 0 = エラー
 　　　　!0 = プログラム開始アドレス
*/
static Long xfile_cnv(Long *prog_size, Long *prog_sz2, Long read_top,
                      void (*onError)(const char *)) {
  if (xhead_getl(0x3C) != 0) {
    onError("BINDされているファイルです\n");
    return (0);
  }

  Long pc_begin = xhead_getl(0x08);
  Long code_size = xhead_getl(0x0C);
  Long data_size = xhead_getl(0x10);
  Long bss_size = xhead_getl(0x14);
  Long reloc_size = xhead_getl(0x18);
  Long textAndData = code_size + data_size;

  if (reloc_size != 0) {
    if (!xrelocate(textAndData, reloc_size, read_top)) {
      onError("未対応のリロケート情報があります\n");
      return (0);
    }
  }

  ULong bss_top = read_top + textAndData;
  if (*prog_sz2 < (Long)(bss_top + bss_size)) {
    onError("メモリが足りません\n");
    return 0;
  }

  Span mem = GetWritableMemorySuper(bss_top, bss_size);
  if (mem.bufptr) memset(mem.bufptr, 0, bss_size);

  *prog_sz2 = textAndData;
  *prog_size = textAndData + bss_size;

  return (read_top + pc_begin);
}

/*
 　機能：プログラムをメモリに読み込む(fpはクローズされる)
 戻り値：正 = 実行開始アドレス
 　　　　負 = エラーコード
*/
Long prog_read(FILE *fp, char *fname, Long read_top, Long *prog_sz,
               Long *prog_sz2, void (*err)(const char *), ExecType execType) {
  Long read_sz;
  bool x_file = false;
  void (*onError)(const char *) = err ? err : onErrorDummy;

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    onError("ファイルのシークに失敗しました\n");
    return DOSE_ILGFMT;
  }
  if ((*prog_sz = ftell(fp)) <= 0) {
    fclose(fp);
    onError("ファイルサイズが０です\n");
    return DOSE_ILGFMT;
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    onError("ファイルのシークに失敗しました\n");
    return DOSE_ILGFMT;
  }
  if (read_top + *prog_sz > *prog_sz2) {
    fclose(fp);
    onError("ファイルサイズが大きすぎます\n");
    return (-8);
  }

  read_sz = *prog_sz;

  Span mem = GetWritableMemorySuper(read_top, read_sz);
  if (!mem.bufptr) {
    return -8;  // ポインタ取得しているだけなので、エラーにはならない
  }
  char *read_ptr = mem.bufptr;

  /* XHEAD_SIZEバイト読み込む */
  if (*prog_sz >= XHEAD_SIZE) {
    if (fread(read_ptr, 1, XHEAD_SIZE, fp) != XHEAD_SIZE) {
      fclose(fp);
      onError("ファイルの読み込みに失敗しました\n");
      return DOSE_ILGFMT;
    }
    read_sz -= XHEAD_SIZE;

    int i;
    if (execType == EXEC_TYPE_R)
      i = 0; /* Rファイル */
    else if (execType == EXEC_TYPE_X)
      i = 1; /* Xファイル */
    else
      i = strlen(fname) - 2;
    if (mem_get(read_top, S_WORD) == 0x4855 && i > 0) {
      if (execType == EXEC_TYPE_X || strcmp(&(fname[i]), ".x") == 0 ||
          strcmp(&(fname[i]), ".X") == 0) {
        x_file = true;
        memcpy(xhead, read_ptr, XHEAD_SIZE);
        *prog_sz = read_sz;
      }
    }
    if (!x_file) {
      // R形式実行ファイルなら最初に読み込んだ64バイトはヘッダではないので、
      // バッファの続きにファイルの続きを読み込む。
      read_ptr += XHEAD_SIZE;
    }
  }

  if (fread(read_ptr, 1, read_sz, fp) != (size_t)read_sz) {
    fclose(fp);
    onError("ファイルの読み込みに失敗しました\n");
    return DOSE_ILGFMT;
  }

  /* 実行ファイルのクローズ */
  fclose(fp);

  /* Xファイルの処理 */
  Long pc_begin = read_top;
  if (x_file) {
    pc_begin = xfile_cnv(prog_sz, prog_sz2, read_top, onError);
    if (pc_begin == 0) return DOSE_ILGFMT;
  } else {
    *prog_sz2 = *prog_sz;
  }

  return (pc_begin);
}

/*
 　機能：xheadからロングデータをゲットする
 戻り値：データの値
*/
static Long xhead_getl(int adr) {
  Long d;

  UByte *p = &(xhead[adr]);

  d = *(p++);
  d = ((d << 8) | *(p++));
  d = ((d << 8) | *(p++));
  d = ((d << 8) | *p);
  return (d);
}

// PSPを作成する
//   事前にBuildMemoryBlock()でメモリブロックを作成しておくこと。
void BuildPsp(ULong psp, ULong envptr, ULong cmdline, UWord parentSr,
              ULong parentSsp, const ProgramSpec *progSpec,
              const Human68kPathName *pathname) {
  Span mem = GetWritableMemorySuper(psp, SIZEOF_PSP);
  if (mem.bufptr) {
    memset(mem.bufptr + SIZEOF_MEMBLK, 0, SIZEOF_PSP - SIZEOF_MEMBLK);
    strcpy(mem.bufptr + PSP_EXEFILE_PATH, pathname->path);
    strcpy(mem.bufptr + PSP_EXEFILE_NAME, pathname->name);
  }

  WriteULongSuper(psp + PSP_ENV_PTR, envptr);
  WriteULongSuper(psp + PSP_CMDLINE, cmdline);
  ULong bssTop = psp + SIZEOF_PSP + progSpec->codeSize;
  WriteULongSuper(psp + PSP_BSS_PTR, bssTop);
  WriteULongSuper(psp + PSP_HEAP_PTR, bssTop);
  WriteULongSuper(psp + PSP_STACK_PTR, bssTop + progSpec->bssSize);

  WriteULongSuper(psp + PSP_PARENT_SSP, parentSsp);
  WriteUWordSuper(psp + PSP_PARENT_SR, parentSr);
}

/* $Id: load.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:08  masamic
 * First imported source code and docs
 *
 * Revision 1.5  1999/12/24  04:04:37  yfujii
 * BUGFIX:When .x or .r is ommited and specified drive or path,
 * run68 couldn't find the executable file.
 *
 * Revision 1.4  1999/12/07  12:47:10  yfujii
 * *** empty log message ***
 *
 * Revision 1.4  1999/11/29  06:11:28  yfujii
 * *** empty log message ***
 *
 * Revision 1.3  1999/10/21  13:32:01  yfujii
 * DOS calls are replaced by win32 functions.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
