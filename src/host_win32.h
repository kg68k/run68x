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

#ifndef HOST_WIN32_H
#define HOST_WIN32_H

#ifdef _WIN32

#include <time.h>

#include "human68k.h"
#include "run68.h"

struct tm* ToLocaltime_win32(const time_t* timer, struct tm* result);
#define HOST_TO_LOCALTIME ToLocaltime_win32

char* Utf8ToSjis2_win32(const char* inbuf, size_t inbytes, size_t* outBufSize);
#define HOST_UTF8_TO_SJIS Utf8ToSjis2_win32

bool CanonicalPathName_win32(const char* path, Human68kPathName* hpn);
#define HOST_CANONICAL_PATHNAME CanonicalPathName_win32

void AddLastSeparator_win32(char* path);
#define HOST_ADD_LAST_SEPARATOR AddLastSeparator_win32

bool PathIsFileSpec_win32(const char* path);
#define HOST_PATH_IS_FILE_SPEC PathIsFileSpec_win32

HostFileInfoMember GetStandardHostfile_win32(int fileno);
#define HOST_GET_STANDARD_HOSTFILE GetStandardHostfile_win32

Long CreateNewfile_win32(char* path, HostFileInfoMember* hostfile,
                         bool newfile);
#define HOST_CREATE_NEWFILE CreateNewfile_win32

Long OpenFile_win32(char* path, HostFileInfoMember* hostfile,
                    FileOpenMode mode);
#define HOST_OPEN_FILE OpenFile_win32

bool CloseFile_win32(FILEINFO* finfop);
#define HOST_CLOSE_FILE CloseFile_win32

Long ReadFileOrTty_win32(FILEINFO* finfop, char* buffer, ULong length);
#define HOST_READ_FILE_OR_TTY ReadFileOrTty_win32

Long SeekFile_win32(FILEINFO* finfop, Long offset, FileSeekMode mode);
#define HOST_SEEK_FILE SeekFile_win32

Long GetFileAtrribute_win32(const char* path);
#define HOST_GET_FILE_ATTRIBUTE GetFileAtrribute_win32

Long SetFileAtrribute_win32(const char* path, UWord atr);
#define HOST_SET_FILE_ATTRIBUTE SetFileAtrribute_win32

Long DosMkdir_win32(Long name);
#define HOST_DOS_MKDIR DosMkdir_win32

Long DosRmdir_win32(Long name);
#define HOST_DOS_RMDIR DosRmdir_win32

Long DosChdir_win32(Long name);
#define HOST_DOS_CHDIR DosChdir_win32

Long DosCurdir_win32(short drv, char* buf_ptr);
#define HOST_DOS_CURDIR DosCurdir_win32

Long DosFiledate_win32(UWord fileno, ULong dt);
#define HOST_DOS_FILEDATE DosFiledate_win32

RegPair IocsOntime_win32(void);
#define HOST_IOCS_ONTIME IocsOntime_win32

#endif
#endif
