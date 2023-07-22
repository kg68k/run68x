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

// インライン関数用
bool mem_red_chk(Long adr);
bool mem_wrt_chk(Long adr);

static inline Word imi_get_word(void) {
  UByte* p = (UByte*)prog_ptr + pc;
  pc += 2;
  return (p[0] << 8) | p[1];
}

// スーパーバイザモードで1ワードのメモリを読む
static inline UWord ReadSuperUWord(ULong adr) {
  adr &= ADDRESS_MASK;
  if ((mem_aloc - 2) < adr) {
    if (!mem_red_chk(adr)) return 0;
  }

  UByte* p = (UByte*)prog_ptr + adr;
  return (p[0] << 8) | p[1];
}

// スーパーバイザモードで1ロングワードのメモリを読む
static inline ULong ReadSuperULong(ULong adr) {
  adr &= ADDRESS_MASK;
  if ((mem_aloc - 4) < adr) {
    if (!mem_red_chk(adr)) return 0;
  }

  UByte* p = (UByte*)prog_ptr + adr;
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

// スーパーバイザモードで1ワードのメモリを書く
static inline void WriteSuperUWord(ULong adr, UWord n) {
  adr &= ADDRESS_MASK;
  if ((mem_aloc - 2) < adr) {
    if (!mem_wrt_chk(adr)) return;
  }

  UByte* p = (UByte*)prog_ptr + adr;
  p[0] = n >> 8;
  p[1] = n;
}

// スーパーバイザモードで1ロングワードのメモリを書く
static inline void WriteSuperULong(ULong adr, ULong n) {
  adr &= ADDRESS_MASK;
  if ((mem_aloc - 4) < adr) {
    if (!mem_wrt_chk(adr)) return;
  }

  UByte* p = (UByte*)prog_ptr + adr;
  p[0] = n >> 24;
  p[1] = n >> 16;
  p[2] = n >> 8;
  p[3] = n;
}

// スタックに積まれたUWord引数を読む(DOSCALL、FEFUNC用)
static inline ULong ReadParamUWord(ULong* paramptr) {
  ULong adr = *paramptr;
  *paramptr += 2;
  return ReadSuperUWord(adr);
}

// スタックに積まれたULong引数を読む(DOSCALL、FEFUNC用)
static inline ULong ReadParamULong(ULong* paramptr) {
  ULong adr = *paramptr;
  *paramptr += 4;
  return ReadSuperULong(adr);
}

#endif