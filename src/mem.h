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

#ifndef MEM_H
#define MEM_H

#include "run68.h"

Long idx_get(void);
Long imi_get(char size);
Long mem_get(Long adr, char size);
void mem_set(Long adr, Long d, char size);
NORETURN void run68_abort(Long adr);

static inline Word imi_get_word(void) {
  UByte* p = (UByte*)prog_ptr + pc;
  pc += 2;
  return (p[0] << 8) | p[1];
}

#endif