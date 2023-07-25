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

#include <direct.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "human68k.h"
#include "run68.h"

// パス名の正規化
bool CanonicalPathName_win32(const char* path, Human68kPathName* hpn) {
  // Human68k仕様に変換できなければエラーにするので、Windowsの仕様より小さくてよい
  char buf[HUMAN68K_PATH_MAX + 1];
  char drive[DRV_CLN_LEN + 1];
  char dir[HUMAN68K_DIR_MAX + 1];
  char fname[HUMAN68K_NAME_MAX + 1];
  char ext[HUMAN68K_NAME_MAX + 1];  // 長い拡張子対策

  if (_fullpath(buf, path, sizeof(buf)) == NULL) return false;
  if (_splitpath_s(buf, drive, sizeof(drive), dir, sizeof(dir), fname,
                   sizeof(fname), ext, sizeof(ext)) != 0) {
    return false;
  }
  if (strlen(drive) != DRV_CLN_LEN) return false;

  size_t nameLen = strlen(fname);
  size_t extLen = strlen(ext);

  if (extLen > HUMAN68K_EXT_MAX) {
    // ".abcd"のような"."+4文字以上の拡張子は、Human68k標準では使用不可だが
    // TwentyOne +P環境では主ファイル名の一部としてなら存在できる。
    // Windowsの_splitpath_s()では拡張子として扱われるので、主ファイル名に繰り込む。
    nameLen += extLen;
    extLen = 0;
    if (nameLen > HUMAN68K_NAME_MAX) return false;
  }

  strcat(strcpy(hpn->path, drive), dir);
  strcat(strcpy(hpn->name, fname), ext);
  hpn->nameLen = nameLen;
  hpn->extLen = extLen;
  return true;
}

// パス名の末尾にパスデリミタを追加する
void AddLastSeparator_win32(char* path) { PathAddBackslashA(path); }

// 文字列にパス区切り文字が含まれないか(ファイル名だけか)を調べる
//   true -> ファイル名のみ
//   false -> ":" や "\" が含まれる
bool PathIsFileSpec_win32(const char* path) {
  return PathIsFileSpecA(path) != FALSE;
}

static HANDLE fileno_to_handle(int fileno) {
  if (fileno == HUMAN68K_STDIN) return GetStdHandle(STD_INPUT_HANDLE);
  if (fileno == HUMAN68K_STDOUT) return GetStdHandle(STD_OUTPUT_HANDLE);
  if (fileno == HUMAN68K_STDERR) return GetStdHandle(STD_ERROR_HANDLE);
  return NULL;
}

// FINFO構造体の環境依存メンバーを初期化する
void InitFileInfo_win32(FILEINFO* finfop, int fileno) {
  finfop->host.handle = fileno_to_handle(fileno);
}

// ファイルを閉じる
bool CloseFile_win32(FILEINFO* finfop) {
  HANDLE hFile = finfop->host.handle;
  if (hFile == NULL) return false;

  finfop->host.handle = NULL;
  return (CloseHandle(hFile) == FALSE) ? false : true;
}

// DOS _MKDIR (0xff39)
Long DosMkdir_win32(Long name) {
  char* name_ptr = prog_ptr + name;

  if (CreateDirectoryA(name_ptr, NULL) == FALSE) {
    if (errno == EACCES) return DOSE_EXISTDIR;  // ディレクトリは既に存在する
    return DOSE_ILGFNAME;                       // ファイル名指定誤り
  }
  return DOSE_SUCCESS;
}

// DOS _RMDIR (0xff3a)
Long DosRmdir_win32(Long name) {
  char* name_ptr = prog_ptr + name;

  errno = 0;
  if (RemoveDirectoryA(name_ptr) == FALSE) {
    if (errno == EACCES)
      return DOSE_NOTEMPTY;  // ディレクトリ中にファイルがある
    return DOSE_ILGFNAME;    // ファイル名指定誤り
  }
  return DOSE_SUCCESS;
}

// DOS _CHDIR (0xff3b)
Long DosChdir_win32(Long name) {
  char* name_ptr = prog_ptr + name;

  if (SetCurrentDirectoryA(name_ptr) == FALSE)
    return DOSE_NODIR;  // ディレクトリが見つからない
  return DOSE_SUCCESS;
}

// DOS _CURDIR (0xff47)
Long DosCurdir_win32(short drv, char* buf_ptr) {
  char buf[HUMAN68K_DRV_DIR_MAX + 1] = {0};
  const char* p = _getdcwd(drv, buf, sizeof(buf));

  if (p == NULL) {
    // Human68kのDOS _CURDIRはエラーコードとして-15しか返さないので
    // _getdcwd()が失敗する理由は考慮しなくてよい。
    return DOSE_ILGDRV;
  }
  strcpy(buf_ptr, p + DRV_CLN_BS_LEN);
  return DOSE_SUCCESS;
}

// DOS _FILEDATE 取得モード
static Long DosFiledate_get(HANDLE hFile) {
  FILETIME ftWrite, ftLocal;
  WORD date, time;

  if (!GetFileTime(hFile, NULL, NULL, &ftWrite)) return DOSE_BADF;
  if (!FileTimeToLocalFileTime(&ftWrite, &ftLocal)) return DOSE_ILGARG;
  if (!FileTimeToDosDateTime(&ftLocal, &date, &time)) return DOSE_ILGARG;

  return ((ULong)date << 16) | time;
}

// DOS _FILEDATE 設定モード
static Long DosFiledate_set(HANDLE hFile, ULong dt) {
  FILETIME ftWrite, ftLocal;

  if (!DosDateTimeToFileTime(dt >> 16, dt & 0xffff, &ftLocal)) return DOSE_BADF;
  if (!LocalFileTimeToFileTime(&ftLocal, &ftWrite)) return DOSE_ILGARG;
  if (!SetFileTime(hFile, NULL, NULL, &ftWrite)) return DOSE_ILGARG;

  return DOSE_SUCCESS;
}

// DOS _FILEDATE (0xff57, 0xff87)
Long DosFiledate_win32(UWord fileno, ULong dt) {
  FILEINFO* finfop = &finfo[fileno];

  if (!finfop->is_opened) return DOSE_BADF;  // オープンされていない

  if (dt == 0) return DosFiledate_get(finfop->host.handle);

  if (finfop->mode == 0)
    return DOSE_ILGARG;  // 読み込みオープンでは設定できない
  return DosFiledate_set(finfop->host.handle, dt);
}
