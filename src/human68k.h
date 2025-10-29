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

#ifndef HUMAN68K_H
#define HUMAN68K_H

#include <stddef.h>

#include "m68k.h"

#define IOCSCALL_ADRS_TABLE 0x0400
#define DOSCALL_ADRS_TABLE 0x1800

// ワークエリア
#define OSWORK_TOP 0x1c00
#define OSWORK_MEMORY_END 0x1c00
#define OSWORK_ROOT_PSP 0x1c04
#define OSWORK_HUMAN_TAIL 0x1c24
#define SIZEOF_OSWORK 1024

// Human68k ver3.02内部アドレス
#define HUMAN_PSP 0x008372
#define NUL_DEVICE_HEADER 0xfa50
#define HUMAN_TAIL 0x01407a

#define HUMAN_MAX_TAIL ((1024 - 64) * 1024)

// デバイスドライバ
typedef struct {
  ULong next;
  UWord attribute;
  ULong strategy;
  ULong interrupt_;
  char name[8];
} DeviceHeader;

#define DEVHEAD_NEXT 0
#define DEVHEAD_ATTRIBUTE 4
#define DEVHEAD_STRATEGY 6
#define DEVHEAD_INTERRUPT 10
#define DEVHEAD_NAME 14

// DOSコールエラー番号
#define DOSE_SUCCESS 0
#define DOSE_ILGFNC -1
#define DOSE_NOENT -2
#define DOSE_NODIR -3
#define DOSE_MFILE -4
#define DOSE_ISDIR -5
#define DOSE_BADF -6
#define DOSE_NOMEM -8
#define DOSE_ILGMPTR -9
#define DOSE_ILGFMT -11
#define DOSE_ILGARG -12
#define DOSE_ILGFNAME -13
#define DOSE_ILGPARM -14
#define DOSE_ILGDRV -15
#define DOSE_RDONLY -19
#define DOSE_EXISTDIR -20
#define DOSE_NOTEMPTY -21
#define DOSE_DISKFULL -23
#define DOSE_CANTSEEK -25
#define DOSE_LCKERR -33
#define DOSE_EXISTFILE -80

// メモリブロック
#define MEMBLK_PREV 0x00
#define MEMBLK_PARENT 0x04
#define MEMBLK_END 0x08
#define MEMBLK_NEXT 0x0c
#define SIZEOF_MEMBLK 16

#define MEMBLK_ALIGN 16

// DOS _MALLOC、_MALLOC2、_SETBLOCKのエラーコード
#define DOSE_MALLOC_NOMEM 0x81000000   // d0 & MALLOC_MASK までなら確保できる
#define DOSE_MALLOC_NOMEM2 0x82000000  // 完全に確保不可
#define MALLOC_MASK 0x00ffffff

// 060turbo.sysのDOS _MALLOC3、_MALLOC4、_SETBLOCK2のエラーコード
#define DOSE_MALLOC3_NOMEM 0x80000000
#define MALLOC3_MASK 0x7fffffff

// DOS _MALLOC2の確保モード
enum {
  MALLOC_FROM_LOWER = 0,
  MALLOC_FROM_SMALLEST = 1,
  MALLOC_FROM_HIGHER = 2,
};

// プロセス管理ポインタ
#define PSP_ENV_PTR 0x10
#define PSP_CMDLINE 0x20
#define PSP_BSS_PTR 0x30
#define PSP_HEAP_PTR 0x34
#define PSP_STACK_PTR 0x38
#define PSP_PARENT_SR 0x44
#define PSP_PARENT_SSP 0x46
#define PSP_SHELL_FLAG 0x60
#define PSP_EXEFILE_PATH 0x80
#define PSP_EXEFILE_NAME 0xc4
#define SIZEOF_PSP 256  // メモリブロックを含む

// 標準ファイルハンドル
#define HUMAN68K_STDIN 0
#define HUMAN68K_STDOUT 1
#define HUMAN68K_STDERR 2
#define HUMAN68K_STDAUX 3
#define HUMAN68K_STDPRN 4
#define HUMAN68K_SYSTEM_FILENO_MAX 4
#define HUMAN68K_USER_FILENO_MIN 5

// パス名関係の定数
#define DRV_CLN_LEN 2     // "A:"
#define DRV_CLN_BS_LEN 3  // "A:\\"

#define HUMAN68K_DIR_MAX 64  // 先頭の"\\"を含む
#define HUMAN68K_NAME_MAX 18
#define HUMAN68K_EXT_MAX 4  // "."を含む

#define HUMAN68K_DRV_DIR_MAX (DRV_CLN_LEN + HUMAN68K_DIR_MAX)
#define HUMAN68K_FILENAME_MAX (HUMAN68K_NAME_MAX + HUMAN68K_EXT_MAX)
#define HUMAN68K_PATH_MAX (HUMAN68K_DRV_DIR_MAX + HUMAN68K_FILENAME_MAX)

// ファイル属性
enum {
  HUMAN68K_FILEATR_READONLY = 0x01,
  HUMAN68K_FILEATR_HIDDEN = 0x02,
  HUMAN68K_FILEATR_SYSTEM = 0x04,
  HUMAN68K_FILEATR_VOLUME = 0x08,
  HUMAN68K_FILEATR_DIRECTORY = 0x10,
  HUMAN68K_FILEATR_ARCHIVE = 0x20,
  HUMAN68K_FILEATR_LINK = 0x40,  // lndrv
  HUMAN68K_FILEATR_EXEC = 0x80,  // execd
};

// パス名正規化バッファ
//   Human68kで使われているものではなく、run68用に定義したもの
typedef struct {
  char path[HUMAN68K_DRV_DIR_MAX + 1];
  char name[HUMAN68K_FILENAME_MAX + 1];
  size_t nameLen, extLen;
} Human68kPathName;

// DOSコール関係の定数
#define SIZEOF_NAMESTS 88
#define SIZEOF_NAMECK 91
#define SIZEOF_FILES 53
#define SIZEOF_FCB 96

// DOS _EXEC
typedef enum {
  EXEC_TYPE_DEFAULT = 0,
  EXEC_TYPE_R = 1,
  EXEC_TYPE_Z = 2,
  EXEC_TYPE_X = 3,
} ExecType;

// DOS _OPEN
typedef enum {
  OPENMODE_READ = 0,
  OPENMODE_WRITE = 1,
  OPENMODE_READ_WRITE = 2,
} FileOpenMode;

// DOS _SEEK
typedef enum {
  SEEKMODE_SET = 0,
  SEEKMODE_CUR = 1,
  SEEKMODE_END = 2,
} FileSeekMode;

// FEFUNC (FLOAT*.X)
#define FEFUNC_FCVT_INT_MAXLEN 255

// Human68kにおける2バイト文字の1バイト目の文字コードか
//   Shift_JIS-2004 ... 0x81～0x9f、0xe0～0xfc
//   Human68kの実際の動作 ... 0x80～0x9f、0xe0～0xff
//     ただしDOS _MAKETMPのみ0x80～0x9f、0xe0～0xefで、これは不具合と思われる。
//   ここではHuman68kの実際の動作を採用する
static inline int is_mb_lead(char c) {
  return (0x80 <= c && c <= 0x9f) || (0xe0 <= c);
}

ULong GetFcbAddress(UWord handle);
int InitHuman68k(ULong humanPSP);

#endif
