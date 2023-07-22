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

#ifndef HUMAN68K_H
#define HUMAN68K_H

// DOSコールエラー番号
#define DOSE_SUCCESS 0
#define DOSE_ILGFNC -1
#define DOSE_NODIR -3
#define DOSE_BADF -6
#define DOSE_ILGMPTR -9
#define DOSE_ILGFNAME -13
#define DOSE_ILGPARM -14
#define DOSE_ILGDRV -15
#define DOSE_RDONLY -19
#define DOSE_EXISTDIR -20
#define DOSE_NOTEMPTY -21

// パス名関係の定数
#define DRV_CLN_LEN 2     // "A:"
#define DRV_CLN_BS_LEN 3  // "A:\\"
#define HUMAN68K_PATH_MAX 65

// 標準ファイルハンドル
#define HUMAN68K_STDIN 0
#define HUMAN68K_STDOUT 1
#define HUMAN68K_STDERR 2
#define HUMAN68K_STDAUX 3
#define HUMAN68K_STDPRN 4
#define HUMAN68K_SYSTEM_FILENO_MAX 4
#define HUMAN68K_USER_FILENO_MIN 5

// メモリブロック
#define MEMBLK_PREV 0x00
#define MEMBLK_PARENT 0x04
#define MEMBLK_END 0x08
#define MEMBLK_NEXT 0x0c
#define SIZEOF_MEMBLK 16

#define MEMBLK_ALIGN 16

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
#define PSP_EXEFILE_DRIVE 0x80
#define PSP_EXEFILE_PATH 0x82
#define PSP_EXEFILE_NAME 0xc4
#define SIZEOF_PSP 256  // メモリブロックを含む

// ワークエリア
#define OSWORK_TOP 0x1c00
#define OSWORK_MEMORY_END 0x1c00
#define OSWORK_ROOT_PSP 0x1c04
#define SIZEOF_OSWORK 1024

#endif
