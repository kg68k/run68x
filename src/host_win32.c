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

#include "human68k.h"
#include "run68.h"

#define DRV_CLN_LEN 2     // "A:"
#define DRV_CLN_BS_LEN 3  // "A:\\"

// DOS _CURDIR
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

// DOS _FILEDATE
Long Filedate_win32(short hdl, Long dt) {
  FILETIME ctime, atime, wtime;
  int64_t ll_wtime;
  HANDLE hFile;
  BOOL b;

  if (finfo[hdl].fh == NULL) return (-6); /* オープンされていない */

  if (dt != 0) { /* 設定 */
    hFile = finfo[hdl].fh;
    GetFileTime(hFile, &ctime, &atime, &wtime);
    ll_wtime = (dt >> 16) * 86400 * 10000000 + (dt & 0xFFFF) * 10000000;
    wtime.dwLowDateTime = (DWORD)(ll_wtime & 0xFFFFFFFF);
    wtime.dwHighDateTime = (DWORD)(ll_wtime >> 32);
    b = SetFileTime(hFile, &ctime, &atime, &wtime);
    if (b) return (-19); /* 書き込み不可 */
    finfo[hdl].date = (ULong)(ll_wtime / 10000000 / 86400);
    finfo[hdl].time = (ULong)((ll_wtime / 10000000) % 86400);
    return (0);
  }

  hFile = finfo[hdl].fh;
  GetFileTime(hFile, &ctime, &atime, &wtime);
  ll_wtime =
      (((int64_t)wtime.dwLowDateTime) << 32) + (int64_t)wtime.dwLowDateTime;
  return (Long)(((ll_wtime / 86400 / 10000000) << 16) +
                (ll_wtime / 10000000) % 86400);
}

#endif
