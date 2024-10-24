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

#include "human68k.h"

#include <string.h>

#include "dos_memory.h"
#include "mem.h"
#include "run68.h"

#define HUMAN_BLOCK_ALIGN (8 * 1024)

static ULong FcbBufferAddress;

static ULong alignBlock(ULong adrs, ULong size) {
  return (adrs + (size - 1)) & ~(size - 1);
}

// Human68kが使用するメモリ領域を確保する
static ULong AllocateHumanMemory(ULong size) {
  const ULong align = 4;

  const ULong tail = ReadULongSuper(OSWORK_HUMAN_TAIL);
  if (tail == 0) {
    print("Human68kのメモリが初期化されていません。");
    return 0;
  }

  const ULong adrs = alignBlock(tail, align);
  const ULong newTail = alignBlock(adrs + size, align);
  if (HUMAN_MAX_TAIL < newTail) {
    print("Human68kのメモリが大きすぎます。");
    return 0;
  }

  WriteULongSuper(OSWORK_HUMAN_TAIL, newTail);
  return adrs;
}

// 指定したデバイスヘッダをメモリに書き込む
static void WriteDrviceHeader(ULong adr, const DeviceHeader dev) {
  WriteULongSuper(adr + DEVHEAD_NEXT, dev.next);
  WriteUWordSuper(adr + DEVHEAD_ATTRIBUTE, dev.attribute);
  WriteULongSuper(adr + DEVHEAD_STRATEGY, dev.strategy);
  WriteULongSuper(adr + DEVHEAD_INTERRUPT, dev.interrupt_);

  Span mem = GetWritableMemorySuper(adr + DEVHEAD_NAME, sizeof(dev.name));
  if (mem.bufptr) memcpy(mem.bufptr, dev.name, mem.length);
}

// 全てのデバイスヘッダをメモリに書き込む
//   ダミーのNULデバイスのみ実装している。
static void WriteDrviceHeaders(void) {
  static const DeviceHeader nuldev = {0xffffffff, 0x8024, 0, 0, "NUL     "};
  WriteDrviceHeader(NUL_DEVICE_HEADER, nuldev);
}

// 不当命令例外のダミー処理アドレスを設定する
static int SetIllegalException(void) {
  static const UByte code[] = {0x43, 0xfa, 0x00, 0x08,  // lea (@f,pc),a1
                               0x70, 0x21, 0x4e, 0x4f,  // IOCS _B_PRINT
                               0xff, 0x00,              // DOS _EXIT

                               // @@: .dc.b 'Illegal Instruction',CR,LF,0
                               0x49, 0x6c, 0x6c, 0x65, 0x67, 0x61, 0x6c, 0x20,
                               0x49, 0x6e, 0x73, 0x74, 0x72, 0x75, 0x63, 0x74,
                               0x69, 0x6f, 0x6e, 0x0d, 0x0a, 0x00};

  ULong adrs = AllocateHumanMemory(sizeof(code));
  if (adrs == 0) return -1;

  Span mem = GetWritableMemorySuper(adrs, sizeof(code));
  if (mem.bufptr) memcpy(mem.bufptr, code, mem.length);

  WriteULongSuper(VECNO_ILLEGAL * 4, adrs);
  DefaultExceptionHandler[VECNO_ILLEGAL] = adrs;

  return 0;
}

static void setExceptionRte(UByte vecno, ULong adrs) {
  // メモリにrte命令を書き込む
  WriteUWordSuper(adrs, 0x4e73);

  // rte命令のアドレスをベクタに書き込む
  WriteULongSuper(vecno * 4, adrs);
  DefaultExceptionHandler[vecno] = adrs;
}

// 例外処理のダミー処理アドレスを設定する
static int SetExceptionHandler(void) {
  ULong mem = AllocateHumanMemory(4);
  if (mem == 0) return -1;

  setExceptionRte(VECNO_ALINE, mem + 0);
  setExceptionRte(VECNO_FLINE, mem + 2);
  return 0;
}

// IOCSコールのダミー処理アドレスを設定する
//   本来Human68kで行う処理ではないがメモリ割り当ての都合上、ここで行う。
static int SetIocsCallHandler(void) {
  const ULong mem = AllocateHumanMemory(4);
  if (mem == 0) return -1;

  // メモリにmoveq #-1,d0とrts命令を書き込む
  WriteULongSuper(mem, 0x70ff4e75);

  for (int i = 0; i < 0x100; i += 1) {
    WriteULongSuper(IOCSCALL_ADRS_TABLE + i * 4, mem);
  }
  return 0;
}

// DOSコールのダミー処理アドレスを設定する
static int SetDosCallHandler(void) {
  const ULong mem = AllocateHumanMemory(4);
  if (mem == 0) return -1;

  // メモリにmoveq #-1,d0とrts命令を書き込む
  WriteULongSuper(mem, 0x70ff4e75);

  for (int i = 0; i < 0x100; i += 1) {
    WriteULongSuper(DOSCALL_ADRS_TABLE + i * 4, mem);
  }
  return 0;
}

// FCB用のバッファを確保する
//   簡易実装。一つ分のバッファしか確保していない。
static int AllocateFcbBuffer(void) {
  const ULong buf = AllocateHumanMemory(SIZEOF_FCB);
  if (buf == 0) return -1;

  FcbBufferAddress = buf;
  return 0;
}

// FCBのアドレスを取得する
//   簡易実装。エラーチェック省略、すべて同一のバッファを返す。
ULong GetFcbAddress(UWord handle) { return FcbBufferAddress; }

// Human68kを初期化する
int InitHuman68k(ULong humanPsp) {
  // Human68kのPSPを作成
  const ULong initialTail = HUMAN_TAIL;
  const ULong humanCodeSize = initialTail - (humanPsp + SIZEOF_PSP);
  BuildMemoryBlock(humanPsp, 0, 0, initialTail, 0);
  const ProgramSpec humanSpec = {humanCodeSize, 0};
  const Human68kPathName humanName = {"A:\\", "HUMAN.SYS", 0, 0};
  BuildPsp(humanPsp, -1, 0, 0x2000, humanPsp, &humanSpec, &humanName);

  WriteULongSuper(OSWORK_ROOT_PSP, humanPsp);
  WriteULongSuper(OSWORK_HUMAN_TAIL, initialTail);

  WriteDrviceHeaders();

  if (SetExceptionHandler() != 0) return -1;
  if (SetIllegalException() != 0) return -1;
  if (SetIocsCallHandler() != 0) return -1;
  if (SetDosCallHandler() != 0) return -1;
  if (AllocateFcbBuffer() != 0) return -1;

  // Human68kの占有するメモリを8KB単位に切り上げる
  ULong humanTail =
      alignBlock(ReadULongSuper(OSWORK_HUMAN_TAIL), HUMAN_BLOCK_ALIGN);
  WriteULongSuper(humanPsp + MEMBLK_END, humanTail);

  // メインメモリの0番地からHuman68kの末尾までをスーパーバイザ領域に設定する。
  // エリアセットレジスタの仕様上は8KB単位。
  SetSupervisorArea(humanTail);

  return 0;
}
