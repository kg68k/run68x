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

#ifndef DOS_FILE_H
#define DOS_FILE_H

#include "run68.h"

Long FindFreeFileNo(void);
Long CreateNewfile(ULong file, UWord atr, bool newfile);
Long OpenExistingFile(ULong file, UWord mode);
Long Read(UWord fileno, ULong buffer, ULong length);
Long Seek(UWord fileno, Long offset, UWord mode);
Long DosChmod(ULong param);
Long Maketmp(ULong path, UWord atr);
void ClearFinfo(int fileno);
FILEINFO* SetFinfo(Long fileno, HostFileInfoMember hostfile, FileOpenMode mode,
                   unsigned int nest);
void FreeOnmemoryFile(FILEINFO* finfop);
void ReadOnmemoryFile(FILEINFO* finfop, FileOpenMode openMode);

#endif
