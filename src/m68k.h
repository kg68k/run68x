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

#ifndef M68K_H
#define M68K_H

#include <stdint.h>

// M680x0 データ型
typedef int8_t Byte;
typedef uint8_t UByte;
typedef int16_t Word;
typedef uint16_t UWord;
typedef int32_t Long;
typedef uint32_t ULong;

typedef struct {
  ULong r0, r1;
} RegPair;

#define ADDRESS_MASK 0x00ffffff

#endif
