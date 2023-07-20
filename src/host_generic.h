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

#ifndef HOST_GENERIC_H
#define HOST_GENERIC_H

#ifndef _WIN32

#include "run68.h"

void InitFileInfo_generic(FILEINFO* finfop, int fileno);
#define HOST_INIT_FILEINFO InitFileInfo_generic

bool CloseFile_generic(FILEINFO* finfop);
#define HOST_CLOSE_FILE CloseFile_generic

Long DosMkdir_generic(Long name);
#define HOST_DOS_MKDIR DosMkdir_generic

Long DosRmdir_generic(Long name);
#define HOST_DOS_RMDIR DosRmdir_generic

Long DosChdir_generic(Long name);
#define HOST_DOS_CHDIR DosChdir_generic

Long DosCurdir_generic(short drv, char* buf_ptr);
#define HOST_DOS_CURDIR DosCurdir_generic

Long DosFiledate_generic(short hdl, Long dt);
#define HOST_DOS_FILEDATE DosFiledate_generic

#endif
#endif
