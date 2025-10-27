// run68x - Human68k CUI Emulator based on run68
// Copyright (C) 2025 TcbnErik
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "host.h"
#include "human68k.h"
#include "mem.h"
#include "run68.h"

struct tm* ToLocaltime_win32(const time_t* timer, struct tm* result) {
  return localtime_s(result, timer) == 0 ? result : NULL;
}

// UTF-8からShift_JISへの変換
static char* utf8ToSjis(const char* inbuf, size_t inbytes, size_t* outBufSize,
                        wchar_t** outWbuf, char** outSjbuf) {
  *outBufSize = 0;
  *outWbuf = NULL;
  *outSjbuf = NULL;

  int wsize =
      MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, inbuf, inbytes, NULL, 0);
  *outWbuf = malloc(sizeof(wchar_t) * wsize);
  if (wsize == 0 || *outWbuf == NULL) return NULL;
  wsize = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, inbuf, inbytes, *outWbuf,
                              wsize);
  if (wsize == 0) return NULL;

  int sjsize =
      WideCharToMultiByte(932, 0, *outWbuf, wsize, NULL, 0, NULL, NULL);
  *outSjbuf = malloc(sjsize);
  if (sjsize == 0 || *outSjbuf == NULL) return NULL;
  sjsize = WideCharToMultiByte(932, 0, *outWbuf, wsize, *outSjbuf, sjsize, NULL,
                               NULL);
  if (sjsize == 0) return NULL;

  *outBufSize = sjsize;
  return *outSjbuf;
}

char* Utf8ToSjis2_win32(const char* inbuf, size_t inbytes, size_t* outBufSize) {
  wchar_t* wbuf;
  char* sjbuf;
  char* buf = utf8ToSjis(inbuf, inbytes, outBufSize, &wbuf, &sjbuf);
  free(wbuf);
  if (!buf) free(sjbuf);
  return buf;
}

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

// 標準入出力ファイルに関するFINFO構造体の環境依存メンバーを返す。
HostFileInfoMember GetStandardHostfile_win32(int fileno) {
  HostFileInfoMember hostfile = {fileno_to_handle(fileno)};
  return hostfile;
}

static Long toDosError(DWORD e, int defaultError) {
  switch (e) {
    default:
      break;

    case ERROR_ACCESS_DENIED:  // ディレクトリを開こうとした場合もこのエラーになる
      return DOSE_RDONLY;
    case ERROR_FILE_NOT_FOUND:
      return DOSE_NOENT;
    case ERROR_PATH_NOT_FOUND:
      return DOSE_NODIR;
    case ERROR_SHARING_VIOLATION:
      return DOSE_LCKERR;
    case ERROR_FILE_EXISTS:
      return DOSE_EXISTFILE;
    case ERROR_DISK_FULL:
      return DOSE_DISKFULL;
    case ERROR_DIRECTORY:
      return DOSE_ISDIR;
  }

  return defaultError;
}

// ファイルを作成する
Long CreateNewfile_win32(char* fullpath, HostFileInfoMember* hostfile,
                         bool newfile) {
  const DWORD dwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
  const DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
  const DWORD dwCreationDisposition = newfile ? CREATE_NEW : CREATE_ALWAYS;

  HANDLE handle =
      CreateFile(fullpath, dwDesiredAccess, dwShareMode, NULL,
                 dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    return toDosError(GetLastError(), DOSE_ILGFNAME);
  }
  hostfile->handle = handle;
  return 0;
}

// ファイルを開く
Long OpenFile_win32(char* fullpath, HostFileInfoMember* hostfile,
                    FileOpenMode mode) {
  static const DWORD md[] = {GENERIC_READ, GENERIC_WRITE,
                             GENERIC_READ | GENERIC_WRITE};
  const DWORD dwDesiredAccess = md[mode];
  const DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
  const DWORD dwCreationDisposition = OPEN_EXISTING;

  HANDLE handle =
      CreateFile(fullpath, dwDesiredAccess, dwShareMode, NULL,
                 dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    return toDosError(GetLastError(), DOSE_NOENT);
  }
  hostfile->handle = handle;
  return 0;
}

// ファイルを閉じる
bool CloseFile_win32(FILEINFO* finfop) {
  HANDLE hFile = finfop->host.handle;
  if (hFile == NULL) return false;

  finfop->host.handle = NULL;
  return (CloseHandle(hFile) == FALSE) ? false : true;
}

// ファイル読み込み
Long ReadFileOrTty_win32(FILEINFO* finfop, char* buffer, ULong length) {
  DWORD read_len;

  if (ReadFile(finfop->host.handle, buffer, length, &read_len, NULL) == FALSE)
    return DOSE_BADF;

  return (Long)read_len;
}

// ファイルシーク
Long SeekFile_win32(FILEINFO* finfop, Long offset, FileSeekMode mode) {
  static const DWORD methods[] = {FILE_BEGIN, FILE_CURRENT, FILE_END};

  DWORD result =
      SetFilePointer(finfop->host.handle, offset, NULL, methods[mode]);
  if (result == INVALID_SET_FILE_POINTER) return DOSE_CANTSEEK;

  return (Long)result;
}

// これらの属性はHuman68kと同じビット位置
enum {
  FILE_ATTRIBUTES_MASK = FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_DIRECTORY |
                         FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN |
                         FILE_ATTRIBUTE_READONLY
};

// ファイル属性の取得: DOS _CHMOD (0xff43)
Long GetFileAtrribute_win32(const char* path) {
  DWORD r = GetFileAttributesA(path);
  if (r == INVALID_FILE_ATTRIBUTES) return DOSE_NOENT;

  return (Long)r & FILE_ATTRIBUTES_MASK;
}

// ファイル属性の設定: DOS _CHMOD (0xff43)
//   シェアリングモードは未対応
Long SetFileAtrribute_win32(const char* path, UWord atr) {
  if (!SetFileAttributesA(path, atr & FILE_ATTRIBUTES_MASK)) {
    DWORD e = GetLastError();
    return toDosError(e, DOSE_ILGFNAME);
  }

  return 0;
}

// DOS _MKDIR (0xff39)
Long DosMkdir_win32(Long name) {
  char* const name_ptr = GetStringSuper(name);

  if (CreateDirectoryA(name_ptr, NULL) == FALSE) {
    if (errno == EACCES) return DOSE_EXISTDIR;  // ディレクトリは既に存在する
    return DOSE_ILGFNAME;                       // ファイル名指定誤り
  }
  return DOSE_SUCCESS;
}

// DOS _RMDIR (0xff3a)
Long DosRmdir_win32(Long name) {
  char* const name_ptr = GetStringSuper(name);

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
  char* const name_ptr = GetStringSuper(name);

  if (SetCurrentDirectoryA(name_ptr) == FALSE)
    return DOSE_NODIR;  // ディレクトリが見つからない
  return DOSE_SUCCESS;
}

static int getDriveNo(short drv) {
  if (drv == 0) return _getdrive();
  if (drv <= 26) return drv;
  return 0;
}

static bool isValidDrive(int drive) {
  if (drive <= 0) return false;

  unsigned long mask = 1UL << (drive - 1);  // bit0=A: bit1=B: ...
  return (_getdrives() & mask) ? true : false;
}

// DOS _CURDIR (0xff47)
Long DosCurdir_win32(short drv, char* buf_ptr) {
  // 無効なドライブに対して_getdcwd()を呼ぶとDebugビルドで
  // ダイアログが表示されてしまうので、除外しておく。
  int drive = getDriveNo(drv);
  if (!isValidDrive(drive)) return DOSE_ILGDRV;

  char buf[HUMAN68K_DRV_DIR_MAX + 1] = {0};
  const char* p = _getdcwd(drive, buf, sizeof(buf));

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

// IOCS _ONTIME (0x7f)
RegPair IocsOntime_win32(void) {
  // GetTickCount64()のミリ秒単位から、IOCS _ONTIMEの1/100秒単位に換算する
  ULONGLONG t = GetTickCount64() / 10;

  lldiv_t r = lldiv(t, 24LL * 60 * 60 * 100);
  return (RegPair){(ULong)r.rem, (ULong)(r.quot & 0xffff)};
}
