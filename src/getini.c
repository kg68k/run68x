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
#include <string.h>

#include "mem.h"
#include "run68.h"

/* 文字列末尾の CR LF を \0 で上書きすることで除去 */
static void chomp(char *buf) {
  while (strlen(buf) != 0 &&
         (buf[strlen(buf) - 1] == '\r' || buf[strlen(buf) - 1] == '\n')) {
    buf[strlen(buf) - 1] = '\0';
  }
}

void read_ini(char *path) {
  char dir[1024] = {0};
  char buf[1024] = {0};
  FILE *fp;
  char *p;

  /* 情報構造体の初期化 */
  ini_info.trap_emulate = false;
  ini_info.pc98_key = false;
  ini_info.io_through = false;
  mem_aloc = DEFAULT_MAIN_MEMORY_SIZE;

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
#ifdef PRINT_RUN68INI_PATH
  printf("INI:%s\n", path);
#endif
  /* フルパス名を使ってファイルをオープンする。*/
  if ((fp = fopen(path, "r")) == NULL) return;

  /* 内容を調べる */
  bool section_match = true;
  while (fgets(buf, 1023, fp) != NULL) {
    chomp(buf);

    /* セクションを見る */
    if (buf[0] == '[') {
      section_match = false;
      if (_stricmp(buf, "[all]") == 0)
        section_match = true;
      continue;
    }

    /* キーワードを見る */
    if (section_match) {
      if (_stricmp(buf, "trapemulate") == 0)
        ini_info.trap_emulate = true;
      else if (_stricmp(buf, "pc98") == 0)
        ini_info.pc98_key = true;
      else if (_stricmp(buf, "iothrough") == 0)
        ini_info.io_through = true;
    }
  }
  fclose(fp);
}

/* run68.iniファイルから環境変数の初期値を取得する。*/
void readenv_from_ini(char *path, ULong envbuf) {
  char buf[1024];
  FILE *fp;
  int len;
  int env_len = 0; /* 環境の長さ */
  const size_t envSize = ReadSuperULong(envbuf);

  /* INIファイルの名前(パス含む)を得る */
  strcpy(buf, path);
  if ((len = strlen(buf)) < 4) return;
  buf[len - 3] = 'i';
  buf[len - 2] = 'n';
  buf[len - 1] = 'i';
  if ((fp = fopen(buf, "r")) == NULL) return;

  /* 内容を調べる */
  bool env_flag = false;  // ファイル先頭は [all] セクション
  while (fgets(buf, 1023, fp) != NULL) {
    chomp(buf);

    /* セクションを見る */
    if (buf[0] == '[') {
      env_flag = false;
      if (_stricmp(buf, "[environment]") == 0) {
        env_flag = true;
      }
      continue;
    }

    if (env_flag) {
      /* 環境変数はiniファイルに記述する。*/
      /* bufに格納された文字列の書式を確認すべきである。*/
      if (env_len + strlen(buf) < envSize - 5) {
        char *mem_ptr = prog_ptr + envbuf + 4 + env_len;
        strcpy(mem_ptr, buf);
#ifdef TRACE
        mem_ptr = prog_ptr + ra[3] + 4 + env_len;
        printf("env: %s\n", mem_ptr);
#endif
        env_len += strlen(buf) + 1;
      }
    }
  }
  *(UByte *)(prog_ptr + envbuf + 4 + env_len) = 0;
  fclose(fp);
}

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
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
