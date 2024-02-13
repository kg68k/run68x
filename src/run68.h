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

#ifndef RUN68_H
#define RUN68_H

#define RUN68X_VERSION "1.3.1-beta.1"
#define RUN68VERSION "0.09a+MacOS"

#if defined(__GNUC__)
#define NORETURN __attribute__((noreturn))
#define GCC_FORMAT(a, b) __attribute__((format(printf, a, b)))
#elif defined(_MSC_VER)
#define NORETURN __declspec(noreturn)
#endif

#ifndef NORETURN
#define NORETURN /* [[noreturn]] */
#endif
#ifndef GCC_FORMAT
#define GCC_FORMAT(a, b)
#endif

#include <limits.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "human68k.h"
#include "m68k.h"

#ifdef _WIN32
#include <windows.h>
#else
#define MAX_PATH PATH_MAX
#define _stricmp strcasecmp
#endif

#if CHAR_MIN != 0
#error "plain 'char' type must be unsigned."
#endif

#define DEFAULT_MAIN_MEMORY_SIZE (12 * 1024 * 1024)
#define DEFAULT_STACK_SIZE (64 * 1024)
#define DEFAULT_ENV_SIZE (8 * 1024)

#define XHEAD_SIZE 0x40     /* Xファイルのヘッダサイズ */
#define HUMAN_HEAD 0x6800   /* Humanのメモリ管理ブロック位置 */
#define FCB_WORK 0x20F00    /* DOSCALL GETFCB用ワーク領域 */
#define HUMAN_WORK 0x21000  /* 割り込み処理先等のワーク領域 */
#define HUMAN_TAIL 0x21C00  // Human68kの末尾 8*1024の倍数であること

#define TRAP0_WORK 0x20FF0000 /* TRAP割り込み処理先等のワーク領域 */
#define TRAP1_WORK 0x21FF0000 /* TRAP割り込み処理先等のワーク領域 */
#define TRAP2_WORK 0x22FF0000 /* TRAP割り込み処理先等のワーク領域 */
#define TRAP3_WORK 0x23FF0000 /* TRAP割り込み処理先等のワーク領域 */
#define TRAP4_WORK 0x24FF0000 /* TRAP割り込み処理先等のワーク領域 */
#define TRAP5_WORK 0x25FF0000 /* TRAP割り込み処理先等のワーク領域 */
#define TRAP6_WORK 0x26FF0000 /* TRAP割り込み処理先等のワーク領域 */
#define TRAP7_WORK 0x27FF0000 /* TRAP割り込み処理先等のワーク領域 */
#define TRAP8_WORK 0x28FF0000 /* TRAP割り込み処理先等のワーク領域 */

#define NEST_MAX 20
#define FILE_MAX 20

#define RAS_INTERVAL 10000 /* ラスタ割り込みの間隔 */

#define S_BYTE 0 /* BYTEサイズ */
#define S_WORD 1 /* WORDサイズ */
#define S_LONG 2 /* LONGサイズ */

#define MD_DD 0 /* データレジスタ直接 */
#define MD_AD 1 /* アドレスレジスタ直接 */
#define MD_AI 2 /* アドレスレジスタ間接 */
#define MD_AIPI 3 /* ポストインクリメント・アドレスレジスタ間接 */
#define MD_AIPD 4 /* プリデクリメント・アドレスレジスタ間接 */
#define MD_AID 5 /* ディスプレースメント付きアドレスレジスタ間接 */
#define MD_AIX 6 /* インデックス付きアドレスレジスタ間接 */
#define MD_OTH 7 /* その他 */

#define MR_SRT 0 /* 絶対ショート */
#define MR_LNG 1 /* 絶対ロング */
#define MR_PC 2  /* プログラムカウンタ相対 */
#define MR_PCX 3 /* インデックス付きプログラムカウンタ相対 */
#define MR_IM 4  /* イミディエイトデータ */

/* Replace from MD_xx, MR_xx */
#define EA_DD 0 /* データレジスタ直接 */
#define EA_AD 1 /* アドレスレジスタ直接 */
#define EA_AI 2 /* アドレスレジスタ間接 */
#define EA_AIPI 3 /* ポストインクリメント・アドレスレジスタ間接 */
#define EA_AIPD 4 /* プリデクリメント・アドレスレジスタ間接 */
#define EA_AID 5 /* ディスプレースメント付きアドレスレジスタ間接 */
#define EA_AIX 6  /* インデックス付きアドレスレジスタ間接 */
#define EA_SRT 7  /* 絶対ショート */
#define EA_LNG 8  /* 絶対ロング */
#define EA_PC 9   /* プログラムカウンタ相対 */
#define EA_PCX 10 /* インデックス付きプログラムカウンタ相対 */
#define EA_IM 11  /* イミディエイトデータ */

/* 選択可能実効アドレス組み合わせ   fedc ba98 7654 3210 */
#define EA_All 0x0fff            /* 0000 1111 1111 1111 */
#define EA_Control 0x07e4        /* 0000 0111 1110 0100 */
#define EA_Data 0x0ffd           /* 0000 1111 1111 1101 */
#define EA_PreDecriment 0x01f4   /* 0000 0001 1111 0100 */
#define EA_PostIncrement 0x07ec  /* 0000 0111 1110 1100 */
#define EA_VariableData 0x01fd   /* 0000 0001 1111 1101 */
#define EA_Variable 0x01ff       /* 0000 0001 1111 1111 */
#define EA_VariableMemory 0x01fc /* 0000 0001 1111 1100 */

#define CCR_X_ON() (sr |= CCR_X)
#define CCR_X_OFF() (sr &= ~CCR_X)
#define CCR_X_REF() (sr & CCR_X)
#define CCR_N_ON() (sr |= CCR_N)
#define CCR_N_OFF() (sr &= ~CCR_N)
#define CCR_N_REF() (sr & CCR_N)
#define CCR_Z_ON() (sr |= CCR_Z)
#define CCR_Z_OFF() (sr &= ~CCR_Z)
#define CCR_Z_REF() (sr & CCR_Z)
#define CCR_V_ON() (sr |= CCR_V)
#define CCR_V_OFF() (sr &= ~CCR_V)
#define CCR_V_REF() (sr & CCR_V)
#define CCR_C_ON() (sr |= CCR_C)
#define CCR_C_OFF() (sr &= ~CCR_C)
#define CCR_C_REF() (sr & CCR_C)
#define SR_S_ON() (sr |= SR_S)
#define SR_S_OFF() (sr &= ~SR_S)
#define SR_S_REF() (sr & SR_S)
#define SR_T_REF() (sr & SR_T1)

#define CCR_X_C_ON() (sr |= (CCR_X | CCR_C))
#define CCR_X_C_OFF() (sr &= ~(CCR_X | CCR_C))

#ifdef _WIN32
typedef struct {
  HANDLE handle;
} HostFileInfoMember;
#else
typedef struct {
  FILE *fp;
} HostFileInfoMember;
#endif

// 全てのメンバーが代入でコピー可能なこと
typedef struct {
  HostFileInfoMember host;
  bool is_opened;
  unsigned date;
  unsigned time;
  short mode;
  unsigned int nest;
  char name[89];
} FILEINFO;

typedef struct {
  bool io_through;
} INI_INFO;

/* デバッグ用に実行した命令の情報を保存しておく構造体 */
typedef struct {
  Long pc;
  /* 本当は全レジスタを保存しておきたい。*/
  unsigned short code; /* OPコード */
  Long rmem;           /* READしたメモリ */
  char rsize; /* B/W/L or N(READなし) movemの場合は最後の一つ */
  Long wmem;  /* WRITEしたメモリ */
  char wsize; /* B/W/L or N(WRITEなし) movemの場合は最後の一つ */
  char mnemonic[64]; /* ニーモニック(できれば) */
} EXEC_INSTRUCTION_INFO;

/* eaaccess.c */
bool get_data_at_ea(int AceptAdrMode, int mode, int reg, int size, Long *data);
bool set_data_at_ea(int AceptAdrMode, int mode, int reg, int size, Long data);
bool get_ea(Long save_pc, int AceptAdrMode, int mode, int reg, Long *data);
bool get_data_at_ea_noinc(int AceptAdrMode, int mode, int reg, int size,
                          Long *data);

/* run68.c */
extern EXEC_INSTRUCTION_INFO OP_info;  // 命令実行情報
extern FILEINFO finfo[FILE_MAX];       // ファイル管理テーブル
extern INI_INFO ini_info;              // iniファイルの内容
extern const char size_char[3];
extern Long ra[8];      // アドレスレジスタ
extern Long rd[8 + 1];  // データレジスタ
extern Long usp;        // USP
extern Long pc;         // プログラムカウンタ
extern UWord sr;        // ステータスレジスタ
extern char *prog_ptr;  // プログラムをロードしたメモリへのポインタ
extern Long superjsr_ret;       // DOSCALL SUPER_JSRの戻りアドレス
extern Long psp[NEST_MAX];      // PSP
extern Long nest_pc[NEST_MAX];  // 親プロセスへの戻りアドレスを保存
extern Long nest_sp[NEST_MAX];  // 親プロセスのスタックポインタを保存
extern unsigned int nest_cnt;  // 子プロセスを起動するたびに+1
extern jmp_buf jmp_when_abort;  // アボート処理のためのジャンプバッファ
extern ULong mem_aloc;     // メインメモリの大きさ(1～12MB)
extern bool func_trace_f;  // -f ファンクションコールトレース
extern Long trap_pc;       // -tr MPU命令トレースを行うアドレス
extern UWord cwatchpoint;  // 命令ウォッチ

void print(const char *message);
void printFmt(const char *fmt, ...) GCC_FORMAT(1, 2);

/* getini.c */
void read_ini(char *path);
void readenv_from_ini(char *path, ULong envbuf);

/* load.c */
typedef struct {
  ULong codeSize;
  ULong bssSize;
} ProgramSpec;

FILE *prog_open(char *, ULong, void (*)(const char *));
Long prog_read(FILE *, char *, Long, Long *, Long *, void (*)(const char *));
void BuildPsp(ULong psp, ULong envptr, ULong cmdline, UWord parentSr,
              ULong parentSsp, const ProgramSpec *progSpec,
              const Human68kPathName *pathname);

/* exec.c */
bool prog_exec(void);
bool get_cond(char);
NORETURN void err68(char *);
NORETURN void err68a(char *mes, char *file, int line);
NORETURN void err68b(char *mes, Long pc, Long ppc);
void inc_ra(int reg, char size);
void dec_ra(int, char size);
void text_color(short);
Long get_locate(void);
void OPBuf_insert(const EXEC_INSTRUCTION_INFO *op);
void OPBuf_clear();
int OPBuf_numentries();
const EXEC_INSTRUCTION_INFO *OPBuf_getentry(int no);
void OPBuf_display(int n);

/* calc.c */
Long add_long(Long src, Long dest, int size);
Long sub_long(Long src, Long dest, int size);

/* doscall.c */
void close_all_files(void);
bool dos_call(UByte);
Long Getenv_common(const char *name_p, char *buf_p, ULong envptr);
Long gets2(char *str, int max);
Long Newfile(char *, short);

/* iocscall.c */
bool iocs_call(void);

/* key.c */
void get_fnckey(int, char *);
void put_fnckey(int, char *);
UByte cnv_key98(UByte);

/* line?.c */
bool line0(char *);
bool line2(char *);
bool line4(char *);
bool line5(char *);
bool line6(char *);
bool line7(char *);
bool line8(char *);
bool line9(char *);
bool lineb(char *);
bool linec(char *);
bool lined(char *);
bool linee(char *);
bool linef(char *);

/* debugger.c */
extern ULong stepcount;

typedef enum {
  RUN68_COMMAND_BREAK,   /* ブレークポイントの設定 */
  RUN68_COMMAND_CLEAR,   /* ブレークポイントのクリア */
  RUN68_COMMAND_CONT,    /* 実行の継続 */
  RUN68_COMMAND_DUMP,    /* メモリをダンプする */
  RUN68_COMMAND_HELP,    /* デバッガのヘルプ */
  RUN68_COMMAND_HISTORY, /* 命令の実行履歴 */
  RUN68_COMMAND_LIST,    /* ディスアセンブル */
  RUN68_COMMAND_NEXT, /* STEPと同じ。ただし、サブルーチン呼出しはスキップ */
  RUN68_COMMAND_QUIT,   /* run68を終了する */
  RUN68_COMMAND_REG,    /* レジスタの内容を表示する */
  RUN68_COMMAND_RUN,    /* 環境を初期化してプログラム実行 */
  RUN68_COMMAND_SET,    /* メモリに値をセットする */
  RUN68_COMMAND_STEP,   /* 一命令分ステップ実行 */
  RUN68_COMMAND_WATCHC, /* 命令ウォッチ */
  RUN68_COMMAND_NULL,   /* コマンドではない(移動禁止) */
  RUN68_COMMAND_ERROR   /* コマンドエラー(移動禁止) */
} RUN68_COMMAND;

RUN68_COMMAND debugger(bool running);

/* conditions.c */
void general_conditions(Long dest, int size);
void add_conditions(Long src, Long dest, Long result, int size, bool zero_flag);
void cmp_conditions(Long src, Long dest, Long result, int size);
void sub_conditions(Long src, Long dest, Long result, int size, bool zero_flag);
void neg_conditions(Long dest, Long result, int size, bool zero_flag);
void check(char *mode, Long src, Long dest, Long result, int size,
           short before);

// dissassemble.c
char *disassemble(Long addr, Long *next_addr);

// 符号拡張
static inline Word extw(Byte b) { return (Word)b; }
static inline Long extbl(Byte b) { return (Long)b; }
static inline Long extl(Word w) { return (Long)w; }

/*
０ライン命令：movep, addi, subi, cmpi, andi, eori, ori, btst, bset, bclr, bchg
１ライン命令：move.b
２ライン命令：move.l, movea.l
３ライン命令：move.w, movea.w
４ライン命令：moveccr, movesr, moveusp, movem, swap, lea, pea, link, unlk,
　　　　　　　clr, ext, neg, negx, tst, tas, not, nbcd, jmp, jsr, rtr, rts,
　　　　　　　trap, trapv, chk, rte, reset, stop, nop
５ライン命令：addq, subq, dbcc, scc
６ライン命令：bcc, bra, bsr
７ライン命令：moveq
８ライン命令：divs, divu, or, sbcd
９ライン命令：sub, suba, subx
Ｂライン命令：cmp, cmpa, cmpm, eor
Ｃライン命令：exg, muls, mulu, and, abcd
Ｄライン命令：add, adda, addx
Ｅライン命令：asl, asr, lsl, lsr, rol, ror, roxl, roxr
*/

#endif

/* $Id: run68.h,v 1.5 2009/08/08 06:49:44 masamic Exp $ */

/*
 * $Log: run68.h,v $
 * Revision 1.5  2009/08/08 06:49:44  masamic
 * Convert Character Encoding Shifted-JIS to UTF-8.
 *
 * Revision 1.4  2009/08/05 14:44:33  masamic
 * Some Bug fix, and implemented some instruction
 * Following Modification contributed by TRAP.
 *
 * Fixed Bug: In disassemble.c, shift/rotate as{lr},ls{lr},ro{lr} alway show
 * word size.
 * Modify: enable KEYSNS, register behaiviour of sub ea, Dn.
 * Add: Nbcd, Sbcd.
 *
 * Revision 1.3  2004/12/17 07:51:06  masamic
 * Support TRAP instraction widely. (but not be tested)
 *
 * Revision 1.2  2004/12/16 12:25:12  masamic
 * It has become under GPL.
 * Maintenor name has changed.
 * Modify codes for aboves.
 *
 * Revision 1.1.1.1  2001/05/23 11:22:08  masamic
 * First imported source code and docs
 *
 * Revision 1.14  1999/12/07  12:47:54  yfujii
 * *** empty log message ***
 *
 * Revision 1.14  1999/11/29  06:24:55  yfujii
 * Some functions' prototypes are added.
 *
 * Revision 1.13  1999/11/08  10:29:30  yfujii
 * Calling convention to eaaccess.c is changed.
 *
 * Revision 1.12  1999/11/08  03:09:41  yfujii
 * Debugger command "wathchc" is added.
 *
 * Revision 1.11  1999/11/01  10:36:33  masamichi
 * Reduced move[a].l routine. and Create functions about accessing effective
 * address.
 *
 * Revision 1.10  1999/11/01  06:23:33  yfujii
 * Some debugging functions are introduced.
 *
 * Revision 1.9  1999/10/29  13:44:04  yfujii
 * Debugging facilities are introduced.
 *
 * Revision 1.8  1999/10/27  03:44:01  yfujii
 * Macro RUN68VERSION is defined.
 *
 * Revision 1.7  1999/10/26  12:26:08  yfujii
 * Environment variable function is drasticaly modified.
 *
 * Revision 1.6  1999/10/26  01:31:54  yfujii
 * Execution history and address trap is added.
 *
 * Revision 1.5  1999/10/25  03:26:27  yfujii
 * Declarations for some flags are added.
 *
 * Revision 1.4  1999/10/20  12:52:10  yfujii
 * Add an #if directive.
 *
 * Revision 1.3  1999/10/20  06:31:09  yfujii
 * Made a little modification for Cygnus GCC.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
