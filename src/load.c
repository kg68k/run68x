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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "host_generic.h"
#include "host_win32.h"
#include "human68k.h"
#include "mem.h"
#include "run68.h"

static UByte xhead[XHEAD_SIZE];

static Long xhead_getl(int);

#ifdef _WIN32
#define PATH_DELIMITER ';'
#else
#define PATH_DELIMITER ':'
#endif

static char *GetAPath(char **path_p, size_t bufSize, char *buf) {
  unsigned int i;
  *buf = '\0';

  if (path_p == NULL || *path_p == NULL) return NULL;

  while ((*path_p)[0] != '\0') {
    const size_t len = strlen(*path_p);
    for (i = 0; i < len && (*path_p)[i] != PATH_DELIMITER; i++) {
      /* 2バイトコードのスキップ */
      ;
    }
    if (i >= (bufSize - 1)) {  // パスデリミタを追加する分を差し引いておく
      *path_p += i + ((*path_p)[0] == '\0') ? 0 : 1;
      continue;
    }

    strncpy(buf, *path_p, i);
    buf[i] = '\0';
    HOST_ADD_LAST_SEPARATOR(buf);

    *path_p = (*path_p)[i] ? &((*path_p)[i]) : &((*path_p)[i + 1]);
    return buf;
  }
  return NULL;
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
FILE *prog_open(char *fname, bool print_error) {
  char dir[MAX_PATH], fullname[MAX_PATH], cwd[MAX_PATH];
  FILE *fp = 0;
  char *exp = strrchr(fname, '.');
  char *p;

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
  HOST_ADD_LAST_SEPARATOR(cwd);
#else
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    fprintf(stderr, "カレントディレクトリのパス名が長すぎます\n");
    return NULL;
  }
#endif
  /* PATH環境変数を取得する */
  char env_p[4096];
#ifdef _WIN32
  Getenv_common("PATH", env_p);
  p = env_p;
#else
  p = getenv("PATH");
  strncpy(env_p, (p == NULL) ? "" : p, sizeof(env_p));
  env_p[sizeof(env_p) - 1] = '\0';
  p = env_p;
#endif
  for (strcpy(dir, cwd); strlen(dir) != 0; GetAPath(&p, sizeof(dir), dir)) {
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
  if (print_error) fprintf(stderr, "ファイルがオープンできません\n");
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
                      bool print_error) {
  Long pc_begin;
  Long code_size;
  Long data_size;
  Long bss_size;
  Long reloc_size;

  if (xhead_getl(0x3C) != 0) {
    if (print_error) fprintf(stderr, "BINDされているファイルです\n");
    return (0);
  }
  pc_begin = xhead_getl(0x08);
  code_size = xhead_getl(0x0C);
  data_size = xhead_getl(0x10);
  bss_size = xhead_getl(0x14);
  reloc_size = xhead_getl(0x18);

  if (reloc_size != 0) {
    if (!xrelocate(code_size + data_size, reloc_size, read_top)) {
      if (print_error) fprintf(stderr, "未対応のリロケート情報があります\n");
      return (0);
    }
  }

  memset(prog_ptr + read_top + code_size + data_size, 0, bss_size);
  *prog_size = code_size + data_size + bss_size;
  *prog_sz2 = code_size + data_size;

  return (read_top + pc_begin);
}

/*
 　機能：プログラムをメモリに読み込む(fpはクローズされる)
 戻り値：正 = 実行開始アドレス
 　　　　負 = エラーコード
*/
Long prog_read(FILE *fp, char *fname, Long read_top, Long *prog_sz,
               Long *prog_sz2, bool print_error)
/* prog_sz2はロードモード＋リミットアドレスの役割も果たす */
{
  char *read_ptr;
  Long read_sz;
  Long pc_begin;
  bool x_file = false;
  int loadmode;
  int i;

  loadmode = ((*prog_sz2 >> 24) & 0x03);
  *prog_sz2 &= 0xFFFFFF;

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    if (print_error) fprintf(stderr, "ファイルのシークに失敗しました\n");
    return (-11);
  }
  if ((*prog_sz = ftell(fp)) <= 0) {
    fclose(fp);
    if (print_error) fprintf(stderr, "ファイルサイズが０です\n");
    return (-11);
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    if (print_error) fprintf(stderr, "ファイルのシークに失敗しました\n");
    return (-11);
  }
  if (read_top + *prog_sz > *prog_sz2) {
    fclose(fp);
    if (print_error) fprintf(stderr, "ファイルサイズが大きすぎます\n");
    return (-8);
  }

  read_sz = *prog_sz;
  read_ptr = prog_ptr + read_top;
  pc_begin = read_top;

  /* XHEAD_SIZEバイト読み込む */
  if (*prog_sz >= XHEAD_SIZE) {
    if (fread(read_ptr, 1, XHEAD_SIZE, fp) != XHEAD_SIZE) {
      fclose(fp);
      if (print_error) fprintf(stderr, "ファイルの読み込みに失敗しました\n");
      return (-11);
    }
    read_sz -= XHEAD_SIZE;
    if (loadmode == 1)
      i = 0; /* Rファイル */
    else if (loadmode == 3)
      i = 1; /* Xファイル */
    else
      i = strlen(fname) - 2;
    if (mem_get(read_top, S_WORD) == 0x4855 && i > 0) {
      if (loadmode == 3 || strcmp(&(fname[i]), ".x") == 0 ||
          strcmp(&(fname[i]), ".X") == 0) {
        x_file = true;
        memcpy(xhead, read_ptr, XHEAD_SIZE);
        *prog_sz = read_sz;
      }
    }
    if (!x_file) read_ptr += XHEAD_SIZE;
  }

  if (fread(read_ptr, 1, read_sz, fp) != (size_t)read_sz) {
    fclose(fp);
    if (print_error) fprintf(stderr, "ファイルの読み込みに失敗しました\n");
    return (-11);
  }

  /* 実行ファイルのクローズ */
  fclose(fp);

  /* Xファイルの処理 */
  *prog_sz2 = *prog_sz;
  if (x_file) {
    if ((pc_begin = xfile_cnv(prog_sz, prog_sz2, read_top, print_error)) == 0)
      return (-11);
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
  memset(&prog_ptr[psp + SIZEOF_MEMBLK], 0, SIZEOF_PSP - SIZEOF_MEMBLK);

  WriteSuperULong(psp + PSP_ENV_PTR, envptr);
  WriteSuperULong(psp + PSP_CMDLINE, cmdline);
  ULong bssTop = psp + SIZEOF_PSP + progSpec->codeSize;
  WriteSuperULong(psp + PSP_BSS_PTR, bssTop);
  WriteSuperULong(psp + PSP_HEAP_PTR, bssTop);
  WriteSuperULong(psp + PSP_STACK_PTR, bssTop + progSpec->bssSize);

  WriteSuperULong(psp + PSP_PARENT_SSP, parentSsp);
  WriteSuperUWord(psp + PSP_PARENT_SR, parentSr);

  strcpy(&prog_ptr[psp + PSP_EXEFILE_PATH], pathname->path);
  strcpy(&prog_ptr[psp + PSP_EXEFILE_NAME], pathname->name);
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
