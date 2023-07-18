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

/* $Id: getini.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:07  masamic
 * First imported source code and docs
 *
 * Revision 1.5  1999/12/07  12:42:44  yfujii
 * *** empty log message ***
 *
 * Revision 1.5  1999/12/01  04:02:55  yfujii
 * .ini file is now retrieved from the same dir as the run68.exe file.
 *
 * Revision 1.4  1999/10/26  12:26:08  yfujii
 * Environment variable function is drasticaly modified.
 *
 * Revision 1.3  1999/10/22  11:06:22  yfujii
 * Expanded emulation memory from 9M to 12M.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN32 a little.
 *
 */

#include <stdio.h>
#include <string.h>

#include "run68.h"

#if !defined(_WIN32) && !defined(DOSX)
#include <ctype.h>
#define _strlwr(p)                         \
  do {                                     \
    char *s;                               \
    for (s = p; *s; s++) *s = tolower(*s); \
  } while (0)
#endif

/* 文字列末尾の CR LF を \0 で上書きすることで除去 */
static void chomp(char *buf) {
  while (strlen(buf) != 0 &&
         (buf[strlen(buf) - 1] == '\r' || buf[strlen(buf) - 1] == '\n')) {
    buf[strlen(buf) - 1] = '\0';
  }
}

void read_ini(char *path, char *prog) {
  char dir[1024] = {0};
  char buf[1024] = {0};
  char sec_name[MAX_PATH];
  FILE *fp;
  int flag = TRUE;
  int i;
  int c;
  char *p;

  /* 情報構造体の初期化 */
  ini_info.env_lower = FALSE;
  ini_info.trap_emulate = FALSE;
  ini_info.pc98_key = FALSE;
  ini_info.io_through = FALSE;
  mem_aloc = 0x100000;

  /* INIファイルのフルパス名を得る。*/
  /* まずはファイル名を取得する。*/
  if ((p = strrchr(path, '\\')) != NULL) {
    memcpy(dir, path, p - path + 1);
    strcpy(buf, p + 1);
  } else if ((p = strrchr(path, '/')) != NULL) {
    memcpy(dir, path, p - path + 1);
    strcpy(buf, p + 1);
  } else if ((p = strrchr(path, ':')) != NULL) {
    memcpy(dir, path, p - path + 1);
    strcpy(buf, p + 1);
  } else {
    strcpy(buf, path);
  }
  /* 拡張子.exeを.iniに置き換える。*/
  if ((p = strrchr(buf, '.')) == NULL) {
    /* 拡張子がついていない時は単に付加する。*/
    strcat(buf, ".ini");
  } else if (_stricmp(p, ".exe") == 0) {
    strcpy(p, ".ini");
  } else {
    return; /* .exe以外の拡張子はないと思う。*/
  }
  /* ディレクトリ名とファイル名を結合する。*/
  snprintf(path, MAX_PATH, "%s%s", dir, buf);
#if defined(_DEBUG)
  printf("INI:%s\n", path);
#endif
  /* フルパス名を使ってファイルをオープンする。*/
  if ((fp = fopen(path, "r")) == NULL) return;
  /* プログラム名を得る */
  for (i = strlen(prog) - 1; i >= 0; i--) {
    if (prog[i] == '\\' || prog[i] == '/' || prog[i] == ':') break;
  }
  i++;
  if (strlen(&(prog[i])) > 22) {
    fclose(fp);
    return;
  }
  sprintf(sec_name, "[%s]", &(prog[i]));
  _strlwr(sec_name);
  /* 内容を調べる */
  while (fgets(buf, 1023, fp) != NULL) {
    _strlwr(buf);
    chomp(buf);

    /* セクションを見る */
    if (buf[0] == '[') {
      flag = FALSE;
      if (_stricmp(buf, "[all]") == 0)
        flag = TRUE;
      else if (_stricmp(buf, sec_name) == 0)
        flag = TRUE;
      continue;
    }

    /* キーワードを見る */
    if (flag == TRUE) {
      if (_stricmp(buf, "envlower") == 0)
        ini_info.env_lower = TRUE;
      else if (_stricmp(buf, "trapemulate") == 0)
        ini_info.trap_emulate = TRUE;
      else if (_stricmp(buf, "pc98") == 0)
        ini_info.pc98_key = TRUE;
      else if (_stricmp(buf, "iothrough") == 0)
        ini_info.io_through = TRUE;
      else if (strncmp(buf, "mainmemory=", 11) == 0) {
        if (strlen(buf) < 12 || 13 < strlen(buf)) continue;
        if ('0' <= buf[11] && buf[11] <= '9') {
          c = buf[11] - '0';
          if (strlen(buf) == 13 && '0' <= buf[12] && buf[12] <= '9') {
            c = c * 10 + buf[11] - '0';
          } else {
            continue;
          }
        } else {
          continue;
        }
        if (1 <= c && c <= 12) mem_aloc = 0x100000 * c;
      }
    }
  }
  fclose(fp);
}

/* run68.iniファイルから環境変数の初期値を取得する。*/
void readenv_from_ini(char *path) {
  char buf[1024];
  FILE *fp;
  int len;
  char *mem_ptr; /* メモリ管理ブロック */
  char *read_ptr;
  int env_len = 0;        /* 環境の長さ */
  BOOL env_flag = FALSE;  // ファイル先頭は [all] セクション

  /* INIファイルの名前(パス含む)を得る */
  strcpy(buf, path);
  if ((len = strlen(buf)) < 4) return;
  buf[len - 3] = 'i';
  buf[len - 2] = 'n';
  buf[len - 1] = 'i';
  if ((fp = fopen(buf, "r")) == NULL) return;

  /* 環境変数はiniファイルに記述する。*/
  mem_set(ra[3], ENV_SIZE, S_LONG);
  mem_set(ra[3] + 4, 0, S_BYTE);
  /* 内容を調べる */
  while (fgets(buf, 1023, fp) != NULL) {
    _strlwr(buf);
    chomp(buf);

    /* セクションを見る */
    if (buf[0] == '[') {
      env_flag = FALSE;
      if (strcmp(buf, "[environment]") == 0) {
        env_flag = TRUE;
      }
      continue;
    }

    if (env_flag == TRUE) {
      /* 環境変数はiniファイルに記述する。*/
      /* bufに格納された文字列の書式を確認すべきである。*/
      if (env_len + strlen(buf) < ENV_SIZE - 5) {
        mem_ptr = prog_ptr + ra[3] + 4 + env_len;
        strcpy(mem_ptr, buf);
        if (ini_info.env_lower == TRUE) {
          _strlwr(buf);
          read_ptr = buf;
          while (*mem_ptr != '\0' && *mem_ptr != '=')
            *(mem_ptr++) = *(read_ptr++);
        }
#ifdef TRACE
        mem_ptr = prog_ptr + ra[3] + 4 + env_len;
        printf("env: %s\n", mem_ptr);
#endif
        env_len += strlen(buf) + 1;
      }
    }
  }
  mem_set(ra[3] + 4 + env_len, 0, S_BYTE);
  fclose(fp);
}
