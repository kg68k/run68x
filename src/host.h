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

#ifndef HOST_H
#define HOST_H

#include "host_misc.h"
#include "host_win32.h"
#include "human68k.h"
#include "run68.h"

#ifndef HOST_CONVERT_TO_SJIS
#ifdef USE_ICONV
#define HOST_CONVERT_TO_SJIS_GENERIC_ICONV
bool Utf8ToSjis_generic_iconv(char* inbuf, char* outbuf, size_t outbuf_size);
#define HOST_CONVERT_TO_SJIS Utf8ToSjis_generic_iconv
#else
#define HOST_CONVERT_TO_SJIS_GENERIC
bool SjisToSjis_generic(char* inbuf, char* outbuf, size_t outbuf_size);
#define HOST_CONVERT_TO_SJIS SjisToSjis_generic
#endif
#endif

#ifndef HOST_CONVERT_FROM_SJIS
#ifdef USE_ICONV
#define HOST_CONVERT_FROM_SJIS_GENERIC_ICONV
bool SjisToUtf8_generic_iconv(char* inbuf, char* outbuf, size_t outbuf_size);
#define HOST_CONVERT_FROM_SJIS SjisToUtf8_generic_iconv
#else
#define HOST_CONVERT_FROM_SJIS_GENERIC
bool SjisToSjis_generic(char* inbuf, char* outbuf, size_t outbuf_size);
#define HOST_CONVERT_FROM_SJIS SjisToSjis_generic
#endif
#endif

#ifndef HOST_CANONICAL_PATHNAME
#define HOST_CANONICAL_PATHNAME_GENERIC
bool CanonicalPathName_generic(const char* path, Human68kPathName* hpn);
#define HOST_CANONICAL_PATHNAME CanonicalPathName_generic
#endif

#ifndef HOST_ADD_LAST_SEPARATOR
#define HOST_ADD_LAST_SEPARATOR_GENERIC
void AddLastSeparator_generic(char* path);
#define HOST_ADD_LAST_SEPARATOR AddLastSeparator_generic
#endif

#ifndef HOST_PATH_IS_FILE_SPEC
#define HOST_PATH_IS_FILE_SPEC_GENERIC
bool PathIsFileSpec_generic(const char* path);
#define HOST_PATH_IS_FILE_SPEC PathIsFileSpec_generic
#endif

#ifndef HOST_INIT_FILEINFO
#define HOST_INIT_FILEINFO_GENERIC
void InitFileInfo_generic(FILEINFO* finfop, int fileno);
#define HOST_INIT_FILEINFO InitFileInfo_generic
#endif

#ifndef HOST_CLOSE_FILE
#define HOST_CLOSE_FILE_GENERIC
bool CloseFile_generic(FILEINFO* finfop);
#define HOST_CLOSE_FILE CloseFile_generic
#endif

#ifndef HOST_READ_FILE_OR_TTY
#define HOST_READ_FILE_OR_TTY_GENERIC
Long ReadFileOrTty_generic(FILEINFO* finfop, char* buffer, ULong length);
#define HOST_READ_FILE_OR_TTY ReadFileOrTty_generic
#endif

#ifndef HOST_DOS_MKDIR
#define HOST_DOS_MKDIR_GENERIC
Long DosMkdir_generic(Long name);
#define HOST_DOS_MKDIR DosMkdir_generic
#endif

#ifndef HOST_DOS_RMDIR
#define HOST_DOS_RMDIR_GENERIC
Long DosRmdir_generic(Long name);
#define HOST_DOS_RMDIR DosRmdir_generic
#endif

#ifndef HOST_DOS_CHDIR
#define HOST_DOS_CHDIR_GENERIC
Long DosChdir_generic(Long name);
#define HOST_DOS_CHDIR DosChdir_generic
#endif

#ifndef HOST_DOS_CURDIR
#define HOST_DOS_CURDIR_GENERIC
Long DosCurdir_generic(short drv, char* buf_ptr);
#define HOST_DOS_CURDIR DosCurdir_generic
#endif

#ifndef HOST_DOS_FILEDATE
#define HOST_DOS_FILEDATE_GENERIC
Long DosFiledate_generic(UWord fileno, ULong dt);
#define HOST_DOS_FILEDATE DosFiledate_generic
#endif

#ifndef HOST_IOCS_ONTIME
#define HOST_IOCS_ONTIME_GENERIC
RegPair IocsOntime_generic(void);
#define HOST_IOCS_ONTIME IocsOntime_generic
#endif

#endif
