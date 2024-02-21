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

#ifndef MEM_H
#define MEM_H

#include "run68.h"

typedef struct {
  char* bufptr;
  ULong address;
  ULong length;
} MemoryRange;

extern char* mainMemoryPtr;

bool AllocateMachineMemory(size_t main_size);
void FreeMachineMemory(void);

void SetSupervisorArea(ULong adr);

bool GetAccessibleMemoryRangeSuper(ULong adr, ULong len, MemoryRange* result);
static inline bool GetReadableMemoryRangeSuper(ULong adr, ULong len,
                                               MemoryRange* result) {
  return GetAccessibleMemoryRangeSuper(adr, len, result);
}
static inline bool GetWritableMemoryRangeSuper(ULong adr, ULong len,
                                               MemoryRange* result) {
  return GetAccessibleMemoryRangeSuper(adr, len, result);
}

char* GetStringSuper(ULong adr);
void WriteStringSuper(ULong adr, const char* s);

NORETURN void throwBusError(ULong adr, bool onWrite);
NORETURN static inline void throwBusErrorOnRead(ULong adr) {
  throwBusError(adr, false);
}
NORETURN static inline void throwBusErrorOnWrite(ULong adr) {
  throwBusError(adr, true);
}

Long idx_get(void);
Long imi_get(char size);
Long mem_get(ULong adr, char size);
void mem_set(ULong adr, Long d, char size);
NORETURN void run68_abort(Long adr);

// インライン関数用
bool mem_red_chk(ULong adr);
bool mem_wrt_chk(ULong adr);

#ifndef __BIG_ENDIAN__
#if defined(_MSC_VER)
#define BYTESWAP16 _byteswap_ushort
#define BYTESWAP32 _byteswap_ulong
#elif defined(__GNUC__)
#define BYTESWAP16 __builtin_bswap16
#define BYTESWAP32 __builtin_bswap32
#endif
#endif

// メインメモリから1バイト読み込む(ビッグエンディアン)
static inline UByte PeekB(ULong adr) { return *(UByte*)(mainMemoryPtr + adr); }

// メインメモリから1ワード読み込む(ビッグエンディアン)
static inline UWord PeekW(ULong adr) {
  UByte* p = (UByte*)(mainMemoryPtr + adr);
#ifdef BYTESWAP16
  return BYTESWAP16(*(UWord*)p);
#else
  return (p[0] << 8) | p[1];
#endif
}

// メインメモリから1ロングワード読み込む(ビッグエンディアン)
static inline ULong PeekL(ULong adr) {
  UByte* p = (UByte*)(mainMemoryPtr + adr);
#ifdef BYTESWAP32
  return BYTESWAP32(*(ULong*)p);
#else
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
#endif
}

// メインメモリに1バイト書き込む(ビッグエンディアン)
static inline void PokeB(ULong adr, UByte n) { *(UByte*)(mainMemoryPtr + adr) = n; }

// メインメモリに1ワード書き込む(ビッグエンディアン)
static inline void PokeW(ULong adr, UWord n) {
  UByte* p = (UByte*)(mainMemoryPtr + adr);
#ifdef BYTESWAP16
  *(UWord*)p = BYTESWAP16(n);
#else
  p[0] = n >> 8;
  p[1] = n;
#endif
}

// メインメモリに1ロングワード書き込む(ビッグエンディアン)
static inline void PokeL(ULong adr, ULong n) {
  UByte* p = (UByte*)(mainMemoryPtr + adr);
#ifdef BYTESWAP32
  *(ULong*)p = BYTESWAP32(n);
#else
  p[0] = n >> 24;
  p[1] = n >> 16;
  p[2] = n >> 8;
  p[3] = n;
#endif
}

static inline Word imi_get_word(void) {
  ULong adr = pc;
  pc += 2;
  return PeekW(adr);
}

// スーパーバイザモードで1バイトのメモリを読む
static inline UWord ReadUByteSuper(ULong adr) {
  adr &= ADDRESS_MASK;
  if (mem_aloc <= adr) {
    if (!mem_red_chk(adr)) return 0;
  }

  return PeekB(adr);
}

// スーパーバイザモードで1ワードのメモリを読む
static inline UWord ReadUWordSuper(ULong adr) {
  adr &= ADDRESS_MASK;
  if ((mem_aloc - 2) < adr) {
    if (!mem_red_chk(adr)) return 0;
  }

  return PeekW(adr);
}

// スーパーバイザモードで1ロングワードのメモリを読む
static inline ULong ReadULongSuper(ULong adr) {
  adr &= ADDRESS_MASK;
  if ((mem_aloc - 4) < adr) {
    if (!mem_red_chk(adr)) return 0;
  }

  return PeekL(adr);
}

// スーパーバイザモードで1バイトのメモリを書く
static inline void WriteUByteSuper(ULong adr, UByte n) {
  adr &= ADDRESS_MASK;
  if ((mem_aloc - 1) < adr) {
    if (!mem_wrt_chk(adr)) return;
  }

  PokeB(adr, n);
}

// スーパーバイザモードで1ワードのメモリを書く
static inline void WriteUWordSuper(ULong adr, UWord n) {
  adr &= ADDRESS_MASK;
  if ((mem_aloc - 2) < adr) {
    if (!mem_wrt_chk(adr)) return;
  }

  PokeW(adr, n);
}

// スーパーバイザモードで1ロングワードのメモリを書く
static inline void WriteULongSuper(ULong adr, ULong n) {
  adr &= ADDRESS_MASK;
  if ((mem_aloc - 4) < adr) {
    if (!mem_wrt_chk(adr)) return;
  }

  PokeL(adr, n);
}

// スタックに積まれたUByte引数を読む(DOSCALLトレース用)
static inline UByte ReadParamUByte(ULong* refParam) {
  UWord v = ReadUByteSuper(*refParam);
  *refParam += 1;
  return v;
}

// スタックに積まれたUWord引数を読む(DOSCALL、FEFUNC用)
static inline ULong ReadParamUWord(ULong* refParam) {
  UWord v = ReadUWordSuper(*refParam);
  *refParam += 2;
  return v;
}

// スタックに積まれたULong引数を読む(DOSCALL、FEFUNC用)
static inline ULong ReadParamULong(ULong* refParam) {
  ULong v = ReadULongSuper(*refParam);
  *refParam += 4;
  return v;
}

#endif
