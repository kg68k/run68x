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

#ifndef DOS_MEMORY_H
#define DOS_MEMORY_H

#include "run68.h"

typedef enum {
  ALLOC_AREA_MAIN_ONLY = 0,
  ALLOC_AREA_HIGH_ONLY = 1,
  ALLOC_AREA_UNLIMITED = 8,
} AllocArea;

typedef struct {
  Long address;
  ULong length;
} MallocResult;

void SetAllocArea(AllocArea area);
MallocResult MallocAll(ULong parent);
Long Malloc(UByte mode, ULong size, ULong parent);
Long MallocHuge(UByte mode, ULong size, ULong parent);
Long Mfree(ULong adr);
Long Setblock(ULong adr, ULong size);
Long SetblockHuge(ULong adr, ULong size);
void BuildMemoryBlock(ULong adrs, ULong prev, ULong parent, ULong end,
                      ULong next);

Long DosMalloc(ULong param);
Long DosMfree(ULong param);
Long DosSetblock(ULong param);
Long DosMalloc2(ULong param);
Long DosMalloc3(ULong param);
Long DosSetblock2(ULong param);
Long DosMalloc4(ULong param);

#endif
