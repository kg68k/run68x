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

// ステータスレジスタ
#define SR_T1 0x8000
// #define SR_T0 0x4000
#define SR_S 0x2000
// #define SR_M 0x1000
#define SR_I2 0x0400
#define SR_I1 0x0200
#define SR_I0 0x0100
#define SR_MASK 0xa700

// コンディションコードレジスタ
#define CCR_X 0x0010
#define CCR_N 0x0008
#define CCR_Z 0x0004
#define CCR_V 0x0002
#define CCR_C 0x0001
#define CCR_MASK 0x001f

// ベクタ番号
#define VECNO_ILLEGAL 0x04
#define VECNO_ALINE 0x0a
#define VECNO_FLINE 0x0b
#define VECNO_TRAP0 0x20
#define VECNO_TRAP1 0x21
#define VECNO_TRAP2 0x22
#define VECNO_TRAP3 0x23
#define VECNO_TRAP4 0x24
#define VECNO_TRAP5 0x25
#define VECNO_TRAP6 0x26
#define VECNO_TRAP7 0x27
#define VECNO_TRAP8 0x28

// 浮動小数点数
typedef union {
  double dbl;
  ULong l[2];
  UByte c[8];
} DBL;

typedef union {
  float flt;
  UByte c[4];
} FLT;

#endif
