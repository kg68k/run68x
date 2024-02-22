// run68x - Human68k CUI Emulator based on run68
// Copyright (C) 2024 TcbnErik
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

#ifndef OPERATE_H
#define OPERATE_H

#include "mem.h"
#include "run68.h"

static inline Word imi_get_word(void) {
  Span mem = GetReadableMemory(pc, 2);
  pc += 2;
  return mem.bufptr ? PeekW(mem.bufptr) : 0;
}

// pcの指すメモリからイミディエートデータを指定サイズで読み込む。
//   サイズに応じてpcを進める。
static inline Long imi_get(char size) {
  ULong len = (size == S_LONG) ? 4 : 2;
  Span mem = GetReadableMemory(pc, len);
  if (!mem.bufptr) throwBusErrorOnRead(pc + mem.length);

  pc += len;
  switch (size) {
    case S_BYTE:
      return PeekB(mem.bufptr + 1);  // 1ワード中の下位バイトが実データ。
    case S_WORD:
      return PeekW(mem.bufptr);
    default:  // S_LONG
      return PeekL(mem.bufptr);
  }
}

// メモリの値を指定サイズで読み込む。
static inline Long mem_get(ULong adr, char size) {
  Span mem = GetReadableMemory(adr, 1 << size);
  if (!mem.bufptr) throwBusErrorOnRead(adr + mem.length);

  switch (size) {
    case S_BYTE:
      return PeekB(mem.bufptr);
    case S_WORD:
      return PeekW(mem.bufptr);
    default:  // S_LONG
      return PeekL(mem.bufptr);
  }
}

// メモリに値を指定サイズで書き込む。
static inline void mem_set(ULong adr, Long d, char size) {
  Span mem = GetWritableMemory(adr, 1 << size);
  if (!mem.bufptr) throwBusErrorOnWrite(adr + mem.length);

  switch (size) {
    case S_BYTE:
      PokeB(mem.bufptr, d);
      return;
    case S_WORD:
      PokeW(mem.bufptr, d);
      return;
    default:  // S_LONG
      PokeL(mem.bufptr, d);
      return;
  }
}

// データレジスタに値を代入する。
static inline void SetDreg(int regno, ULong n, int size) {
  switch (size) {
    case S_BYTE:
      rd[regno] = (rd[regno] & 0xffffff00) | (n & 0xff);
      break;
    case S_WORD:
      rd[regno] = (rd[regno] & 0xffff0000) | (n & 0xffff);
      break;
    default:  // S_LONG
      rd[regno] = n;
      break;
  }
}

#endif
