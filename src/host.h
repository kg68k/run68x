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

#ifndef HOST_H
#define HOST_H

#include <time.h>

#include "host_win32.h"
#include "human68k.h"
#include "run68.h"

#ifndef HOST_TO_LOCALTIME
#define HOST_TO_LOCALTIME_GENERIC
struct tm* ToLocaltime_generic(const time_t* timer, struct tm* result);
#define HOST_TO_LOCALTIME ToLocaltime_generic
#endif

#ifndef HOST_UTF8_TO_SJIS
#ifdef USE_ICONV
#define HOST_UTF8_TO_SJIS_GENERIC_ICONV
char* Utf8ToSjis2_generic_iconv(const char* inbuf, size_t inbytes,
                                size_t* outBufSize);
#define HOST_UTF8_TO_SJIS Utf8ToSjis2_generic_iconv
#else
#define HOST_UTF8_TO_SJIS(inbuf, inbytes, outBufSize) (NULL)
#endif
#endif

#ifndef HOST_CONVERT_TO_SJIS
#ifdef USE_ICONV
#define HOST_CONVERT_TO_SJIS_GENERIC_ICONV
bool Utf8ToSjis_generic_iconv(const char* inbuf, char* outbuf,
                              size_t outbuf_size);
#define HOST_CONVERT_TO_SJIS Utf8ToSjis_generic_iconv
#else
#define HOST_CONVERT_TO_SJIS_GENERIC
bool SjisToSjis_generic(const char* inbuf, char* outbuf, size_t outbuf_size);
#define HOST_CONVERT_TO_SJIS SjisToSjis_generic
#endif
#endif

#ifndef HOST_CONVERT_FROM_SJIS
#ifdef USE_ICONV
#define HOST_CONVERT_FROM_SJIS_GENERIC_ICONV
bool SjisToUtf8_generic_iconv(const char* inbuf, char* outbuf,
                              size_t outbuf_size);
#define HOST_CONVERT_FROM_SJIS SjisToUtf8_generic_iconv
#else
#define HOST_CONVERT_FROM_SJIS_GENERIC
bool SjisToSjis_generic(const char* inbuf, char* outbuf, size_t outbuf_size);
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

#ifndef HOST_GET_STANDARD_HOSTFILE
#define HOST_GET_STANDARD_HOSTFILE_GENERIC
HostFileInfoMember GetStandardHostfile_generic(int fileno);
#define HOST_GET_STANDARD_HOSTFILE GetStandardHostfile_generic
#endif

#ifndef HOST_CREATE_NEWFILE
#define HOST_CREATE_NEWFILE_GENERIC
Long CreateNewfile_generic(char* path, HostFileInfoMember* hostfile,
                           bool newfile);
#define HOST_CREATE_NEWFILE CreateNewfile_generic
#endif

#ifndef HOST_OPEN_FILE
#define HOST_OPEN_FILE_GENERIC
Long OpenFile_generic(char* path, HostFileInfoMember* hostfile,
                      FileOpenMode mode);
#define HOST_OPEN_FILE OpenFile_generic
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

#ifndef HOST_SEEK_FILE
#define HOST_SEEK_FILE_GENERIC
Long SeekFile_generic(FILEINFO* finfop, Long offset, FileSeekMode mode);
#define HOST_SEEK_FILE SeekFile_generic
#endif

#ifndef HOST_GET_FILE_ATTRIBUTE
#define HOST_GET_FILE_ATTRIBUTE_GENERIC
Long GetFileAtrribute_generic(const char* path);
#define HOST_GET_FILE_ATTRIBUTE GetFileAtrribute_generic
#endif

#ifndef HOST_SET_FILE_ATTRIBUTE
#define HOST_SET_FILE_ATTRIBUTE_GENERIC
Long SetFileAtrribute_generic(const char* path, UWord atr);
#define HOST_SET_FILE_ATTRIBUTE SetFileAtrribute_generic
#endif

#ifndef HOST_MKDIR
#define HOST_MKDIR_GENERIC
Long Mkdir_generic(const char* dirname);
#define HOST_MKDIR Mkdir_generic
#endif

#ifndef HOST_RMDIR
#define HOST_RMDIR_GENERIC
Long Rmdir_generic(const char* dirname);
#define HOST_RMDIR Rmdir_generic
#endif

#ifndef HOST_CHDIR
#define HOST_CHDIR_GENERIC
Long Chdir_generic(const char* dirname);
#define HOST_CHDIR Chdir_generic
#endif

#ifndef HOST_CURDIR
#define HOST_CURDIR_GENERIC
Long Curdir_generic(UWord drive, char* buf);
#define HOST_CURDIR Curdir_generic
#endif

#ifndef HOST_GET_FILEDATE
#define HOST_GET_FILEDATE_GENERIC
Long GetFiledate_generic(FILEINFO* finfop);
#define HOST_GET_FILEDATE GetFiledate_generic
#endif

#ifndef HOST_SET_FILEDATE
#define HOST_SET_FILEDATE_GENERIC
Long SetFiledate_generic(FILEINFO* finfop, ULong dt);
#define HOST_SET_FILEDATE SetFiledate_generic
#endif

#ifndef HOST_IOCS_ONTIME
#define HOST_IOCS_ONTIME_GENERIC
RegPair IocsOntime_generic(void);
#define HOST_IOCS_ONTIME IocsOntime_generic
#endif

#endif
