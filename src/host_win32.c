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

#ifdef _WIN32

#include <direct.h>
#include <string.h>
#include <windows.h>

#include "human68k.h"
#include "run68.h"

#define DRV_CLN_LEN 2     // "A:"
#define DRV_CLN_BS_LEN 3  // "A:\\"

// オープン中のファイルハンドルか
bool IsOpendFile_win32(FILEINFO* finfop) {
  return (finfop->handle == NULL) ? true : false;
}

// ファイルを閉じる
bool CloseFile_win32(FILEINFO* finfop) {
  HANDLE handle = finfop->handle;
  if (handle == NULL) return false;

  finfop->handle = NULL;
  return (CloseHandle(handle) == FALSE) ? false : true;
}

// DOS _MKDIR (0xff39)
Long Mkdir_win32(Long name) {
  char* name_ptr = prog_ptr + name;

  if (CreateDirectoryA(name_ptr, NULL) == FALSE) {
    if (errno == EACCES) return DOSE_EXISTDIR;  // ディレクトリは既に存在する
    return DOSE_ILGFNAME;                       // ファイル名指定誤り
  }
  return DOSE_SUCCESS;
}

// DOS _RMDRV (0xff3a)
Long Rmdir_win32(Long name) {
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
Long Chdir_win32(Long name) {
  char* name_ptr = prog_ptr + name;

  if (SetCurrentDirectoryA(name_ptr) == FALSE)
    return DOSE_NODIR;  // ディレクトリが見つからない
  return DOSE_SUCCESS;
}

// DOS _CURDIR (0xff47)
Long Curdir_win32(short drv, char* buf_ptr) {
  char buf[DRV_CLN_LEN + HUMAN68K_PATH_MAX] = {0};
  const char* p = _getdcwd(drv, buf, sizeof(buf));

  if (p == NULL) {
    // Human68kのDOS _CURDIRはエラーコードとして-15しか返さないので
    // _getdcwd()が失敗する理由は考慮しなくてよい。
    return DOSE_ILGDRV;
  }
  strcpy(buf_ptr, p + DRV_CLN_BS_LEN);
  return DOSE_SUCCESS;
}

// DOS _FILEDATE (0xff87)
Long Filedate_win32(short hdl, Long dt) {
  FILETIME ctime, atime, wtime;
  int64_t ll_wtime;

  if (!IsOpendFile_win32(&finfo[hdl]))
    return DOSE_BADF;  // オープンされていない

  HANDLE hFile = finfo[hdl].handle;

  if (dt != 0) { /* 設定 */
    GetFileTime(hFile, &ctime, &atime, &wtime);
    ll_wtime = (dt >> 16) * 86400 * 10000000 + (dt & 0xFFFF) * 10000000;
    wtime.dwLowDateTime = (DWORD)(ll_wtime & 0xFFFFFFFF);
    wtime.dwHighDateTime = (DWORD)(ll_wtime >> 32);
    if (SetFileTime(hFile, &ctime, &atime, &wtime) == FALSE) {
      // 書き込み不可
      // 実際のHuman68kでは-19が返ることはない。
      return DOSE_RDONLY;
    }
    finfo[hdl].date = (ULong)(ll_wtime / 10000000 / 86400);
    finfo[hdl].time = (ULong)((ll_wtime / 10000000) % 86400);
    return DOSE_SUCCESS;
  }

  GetFileTime(hFile, &ctime, &atime, &wtime);
  ll_wtime =
      (((int64_t)wtime.dwLowDateTime) << 32) + (int64_t)wtime.dwLowDateTime;
  return (Long)(((ll_wtime / 86400 / 10000000) << 16) +
                (ll_wtime / 10000000) % 86400);
}

#endif
