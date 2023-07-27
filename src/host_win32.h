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

#ifndef HOST_WIN32_H
#define HOST_WIN32_H

#ifdef _WIN32

#include "human68k.h"
#include "run68.h"

bool CanonicalPathName_win32(const char* path, Human68kPathName* hpn);
#define HOST_CANONICAL_PATHNAME CanonicalPathName_win32

void AddLastSeparator_win32(char* path);
#define HOST_ADD_LAST_SEPARATOR AddLastSeparator_win32

bool PathIsFileSpec_win32(const char* path);
#define HOST_PATH_IS_FILE_SPEC PathIsFileSpec_win32

void InitFileInfo_win32(FILEINFO* finfop, int fileno);
#define HOST_INIT_FILEINFO InitFileInfo_win32

bool CloseFile_win32(FILEINFO* finfop);
#define HOST_CLOSE_FILE CloseFile_win32

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
