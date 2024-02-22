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
  ULong length;
} Span;

extern char* mainMemoryPtr;
extern ULong mainMemoryEnd;
extern ULong supervisorEnd;

bool AllocateMachineMemory(const Settings* settings);
void FreeMachineMemory(void);

void SetSupervisorArea(ULong adr);

// 論理アドレスを物理アドレスに変換する。
static inline ULong toPhysicalAddress(ULong adr) { return adr & ADDRESS_MASK; }

// 指定したメモリ範囲がアクセス可能か調べ、バッファアドレスを返す。
//   アクセス不可能なら(Span){NULL, (先頭からのアクセス可能なバイト数)}を返す。
//   現在のところ読み書きを区別しない。
static inline Span getAccessibleMemory(ULong adr, ULong len, bool super) {
  adr = toPhysicalAddress(adr);
  const ULong end = mainMemoryEnd;

  if (adr < end) {
    // 開始アドレスはメインメモリ内。
    if (!super && adr < supervisorEnd) {
      // ユーザーモードでスーパーバイザ領域をアクセスしようとした。
      return (Span){NULL, 0};
    }
  } else {
    // メインメモリ外は全てアクセス不可能。
    // 実機の仕様としてはGVRAM等は読み書きできるが、run68では実装していない。
    return (Span){NULL, 0};
  }

  if (end < (adr + len)) {
    // 指定範囲の途中までアクセス可能。
    return (Span){NULL, end - adr};
  }

  // 指定範囲はすべてアクセス可能。
  return (Span){mainMemoryPtr + adr, len};
}

static inline Span GetReadableMemorySuper(ULong adr, ULong len) {
  return getAccessibleMemory(adr, len, true);
}
static inline Span GetWritableMemorySuper(ULong adr, ULong len) {
  return getAccessibleMemory(adr, len, true);
}
static inline Span GetReadableMemory(ULong adr, ULong len) {
  bool super = SR_S_REF() ? true : false;
  return getAccessibleMemory(adr, len, super);
}
static inline Span GetWritableMemory(ULong adr, ULong len) {
  bool super = SR_S_REF() ? true : false;
  return getAccessibleMemory(adr, len, super);
}

bool getAccessibleMemoryRange(ULong adr, ULong len, bool super, Span* result);

static inline bool GetReadableMemoryRangeSuper(ULong adr, ULong len,
                                               Span* result) {
  return getAccessibleMemoryRange(adr, len, true, result);
}
static inline bool GetWritableMemoryRangeSuper(ULong adr, ULong len,
                                               Span* result) {
  return getAccessibleMemoryRange(adr, len, true, result);
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

#ifndef __BIG_ENDIAN__
#if defined(_MSC_VER)
#define BYTESWAP16 _byteswap_ushort
#define BYTESWAP32 _byteswap_ulong
#elif defined(__GNUC__)
#define BYTESWAP16 __builtin_bswap16
#define BYTESWAP32 __builtin_bswap32
#endif
#endif

// メモリから1バイト読み込む(ビッグエンディアン)
static inline UByte PeekB(char* ptr) { return *(UByte*)ptr; }

// メモリから1ワード読み込む(ビッグエンディアン)
static inline UWord PeekW(char* ptr) {
#ifdef BYTESWAP16
  return BYTESWAP16(*(UWord*)ptr);
#else
  UByte* p = (UByte*)ptr;
  return (p[0] << 8) | p[1];
#endif
}

// メモリから1ロングワード読み込む(ビッグエンディアン)
static inline ULong PeekL(char* ptr) {
#ifdef BYTESWAP32
  return BYTESWAP32(*(ULong*)ptr);
#else
  UByte* p = (UByte*)ptr;
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
#endif
}

// メモリに1バイト書き込む(ビッグエンディアン)
static inline void PokeB(char* ptr, UByte n) { *(UByte*)ptr = n; }

// メモリに1ワード書き込む(ビッグエンディアン)
static inline void PokeW(char* ptr, UWord n) {
#ifdef BYTESWAP16
  *(UWord*)ptr = BYTESWAP16(n);
#else
  UByte* p = (UByte*)ptr;
  p[0] = n >> 8;
  p[1] = n;
#endif
}

// メモリに1ロングワード書き込む(ビッグエンディアン)
static inline void PokeL(char* ptr, ULong n) {
#ifdef BYTESWAP32
  *(ULong*)ptr = BYTESWAP32(n);
#else
  UByte* p = (UByte*)ptr;
  p[0] = n >> 24;
  p[1] = n >> 16;
  p[2] = n >> 8;
  p[3] = n;
#endif
}

// スーパーバイザモードで1バイトのメモリを読む
static inline UWord ReadUByteSuper(ULong adr) {
  Span mem = GetReadableMemorySuper(adr, 1);
  if (!mem.bufptr) throwBusErrorOnRead(adr + mem.length);

  return PeekB(mem.bufptr);
}

// スーパーバイザモードで1ワードのメモリを読む
static inline UWord ReadUWordSuper(ULong adr) {
  Span mem = GetReadableMemorySuper(adr, 2);
  if (!mem.bufptr) throwBusErrorOnRead(adr + mem.length);

  return PeekW(mem.bufptr);
}

// スーパーバイザモードで1ロングワードのメモリを読む
static inline ULong ReadULongSuper(ULong adr) {
  Span mem = GetReadableMemorySuper(adr, 4);
  if (!mem.bufptr) throwBusErrorOnRead(adr + mem.length);

  return PeekL(mem.bufptr);
}

// スーパーバイザモードで1バイトのメモリを書く
static inline void WriteUByteSuper(ULong adr, UByte n) {
  Span mem = GetWritableMemorySuper(adr, 1);
  if (!mem.bufptr) throwBusErrorOnWrite(adr + mem.length);

  PokeB(mem.bufptr, n);
}

// スーパーバイザモードで1ワードのメモリを書く
static inline void WriteUWordSuper(ULong adr, UWord n) {
  Span mem = GetWritableMemorySuper(adr, 2);
  if (!mem.bufptr) throwBusErrorOnWrite(adr + mem.length);

  PokeW(mem.bufptr, n);
}

// スーパーバイザモードで1ロングワードのメモリを書く
static inline void WriteULongSuper(ULong adr, ULong n) {
  Span mem = GetWritableMemorySuper(adr, 4);
  if (!mem.bufptr) throwBusErrorOnWrite(adr + mem.length);

  PokeL(mem.bufptr, n);
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

Long imi_get(char size);
Long mem_get(ULong adr, char size);
void mem_set(ULong adr, Long d, char size);

static inline Word imi_get_word(void) {
  Span mem = GetReadableMemory(pc, 2);
  pc += 2;
  return mem.bufptr ? PeekW(mem.bufptr) : 0;
}

#endif
