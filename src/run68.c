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

#include "run68.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dos_memory.h"
#include "host.h"
#include "human68k.h"
#include "hupair.h"
#include "mem.h"

EXEC_INSTRUCTION_INFO OP_info;
FILEINFO finfo[FILE_MAX];
INI_INFO ini_info;
const char size_char[3] = {'b', 'w', 'l'};
Long ra[8];
Long rd[8 + 1];
Long usp;
Long pc;
UWord sr;
char *prog_ptr;
int trap_count;
Long superjsr_ret;
Long psp[NEST_MAX];
Long nest_pc[NEST_MAX];
Long nest_sp[NEST_MAX];
unsigned int nest_cnt;
jmp_buf jmp_when_abort;
ULong mem_aloc;
bool func_trace_f = false;
Long trap_pc;
UWord cwatchpoint = 0x4afc;

static bool trace_f = false;
static bool debug_on = false;
static bool debug_flag = false;

static char ini_file_name[MAX_PATH];

static void print_title(void) {
  const char *title =  //
      "run68x " RUN68X_VERSION
#ifdef _DEBUG
      " (debug)"
#endif
      "  Copyright (C) 2024 TcbnErik\n"
      "  based on X68000 console emulator Ver." RUN68VERSION
      "\n"
      "          Created in 1996 by Yokko\n"
      "          Maintained since Oct. 1999 by masamic and Chack'n\n";
  print(title);
}

static void print_usage(void) {
  const char *usage =
      "Usage: run68 [options] execute_filename [commandline]\n"
      "    -f        function call trace\n"
      "    -t        mpu trace\n"
      "    -debug    run with debugger\n";
  print(usage);
}

/*
   機能：割り込みベクタテーブルを作成する
 戻り値：なし
*/
static void trap_table_make(void) {
  int i;

  SR_S_ON();

  /* 割り込みルーチンの処理先設定 */
  mem_set(0x28, HUMAN_WORK, S_LONG);       /* A系列命令 */
  mem_set(0x2C, HUMAN_WORK, S_LONG);       /* F系列命令 */
  mem_set(0x80, TRAP0_WORK, S_LONG);       /* trap0 */
  mem_set(0x84, TRAP1_WORK, S_LONG);       /* trap1 */
  mem_set(0x88, TRAP2_WORK, S_LONG);       /* trap2 */
  mem_set(0x8C, TRAP3_WORK, S_LONG);       /* trap3 */
  mem_set(0x90, TRAP4_WORK, S_LONG);       /* trap4 */
  mem_set(0x94, TRAP5_WORK, S_LONG);       /* trap5 */
  mem_set(0x98, TRAP6_WORK, S_LONG);       /* trap6 */
  mem_set(0x9C, TRAP7_WORK, S_LONG);       /* trap7 */
  mem_set(0xA0, TRAP8_WORK, S_LONG);       /* trap8 */
  mem_set(0x118, 0, S_LONG);               /* vdisp */
  mem_set(0x138, 0, S_LONG);               /* crtc */
  mem_set(HUMAN_WORK, 0x4e73, S_WORD);     /* 0x4e73 = rte */
  mem_set(HUMAN_WORK + 2, 0x4e75, S_WORD); /* 0x4e75 = rts */

  /* IOCSコールベクタの設定 */
  for (i = 0; i < 256; i++) {
    mem_set(0x400 + i * 4, HUMAN_WORK + 2, S_LONG);
  }

  /* IOCSワークの設定 */
  mem_set(0x970, 79, S_WORD); /* 画面の桁数-1 */
  mem_set(0x972, 24, S_WORD); /* 画面の行数-1 */

  /* DOSコールベクタの設定 */
  for (i = 0; i < 256; i++) {
    mem_set(0x1800 + i * 4, HUMAN_WORK + 2, S_LONG);
  }

  SR_S_OFF();
}

// 指定したデバイスヘッダをメモリに書き込む
static void WriteDrviceHeader(ULong adr, const DeviceHeader dev) {
  WriteSuperULong(adr + DEVHEAD_NEXT, dev.next);
  WriteSuperUWord(adr + DEVHEAD_ATTRIBUTE, dev.attribute);
  WriteSuperULong(adr + DEVHEAD_STRATEGY, dev.strategy);
  WriteSuperULong(adr + DEVHEAD_INTERRUPT, dev.interrupt_);

  MemoryRange mem;
  if (GetWritableMemoryRange(adr + DEVHEAD_NAME, sizeof(dev.name), &mem))
    memcpy(mem.bufptr, dev.name, mem.length);
}

// 全てのデバイスヘッダをメモリに書き込む
//   ダミーのNULデバイスのみ実装している。
static void WriteDrviceHeaders(void) {
  static const DeviceHeader nuldev = {0xffffffff, 0x8024, 0, 0, "NUL     "};
  WriteDrviceHeader(NUL_DEVICE_HEADER, nuldev);
}

/*
 　機能：割り込みをエミュレートして実行する
 戻り値：なし
*/
static int exec_trap(bool *restart) {
  Long trap_adr;
  static bool cont_flag = true;
  static bool running = true;

  trap_count = 1;
  UByte *const trap_mem1 = (UByte *)prog_ptr + 0x118;
  UByte *const trap_mem2 = (UByte *)prog_ptr + 0x138;
  OPBuf_clear();
  do {
    /* 実行した命令の情報を保存しておく */
    OP_info.pc = 0;
    OP_info.code = 0;
    OP_info.rmem = 0;
    OP_info.rsize = 'N';
    OP_info.wmem = 0;
    OP_info.wsize = 'N';
    OP_info.mnemonic[0] = 0;
    if (superjsr_ret == pc) {
      SR_S_OFF();
      superjsr_ret = 0;
    }
    if (trap_count == 1) {
      if (*((Long *)trap_mem1) != 0) {
        trap_adr = *trap_mem1;
        trap_adr = ((trap_adr << 8) | *(trap_mem1 + 1));
        trap_adr = ((trap_adr << 8) | *(trap_mem1 + 2));
        trap_adr = ((trap_adr << 8) | *(trap_mem1 + 3));
        trap_count = 0;
        ra[7] -= 4;
        mem_set(ra[7], pc, S_LONG);
        ra[7] -= 2;
        mem_set(ra[7], sr, S_WORD);
        pc = trap_adr;
        SR_S_ON();
      } else if (*((Long *)trap_mem2) != 0) {
        trap_adr = *trap_mem2;
        trap_adr = ((trap_adr << 8) | *(trap_mem2 + 1));
        trap_adr = ((trap_adr << 8) | *(trap_mem2 + 2));
        trap_adr = ((trap_adr << 8) | *(trap_mem2 + 3));
        trap_count = 0;
        ra[7] -= 4;
        mem_set(ra[7], pc, S_LONG);
        ra[7] -= 2;
        mem_set(ra[7], sr, S_WORD);
        pc = trap_adr;
        SR_S_ON();
      }
    } else {
      if (trap_count > 1) trap_count--;
    }
    if (trap_pc != 0 && pc == trap_pc) {
      printFmt("(run68) trapped:MPUがアドレス$%06Xの命令を実行しました。\n",
               pc);
      debug_on = true;
      if (stepcount != 0) {
        printFmt("(run68) breakpoint:%d counts left.\n", stepcount);
        stepcount = 0;
      }
    } else if (cwatchpoint != 0x4afc && cwatchpoint == PeekW(pc)) {
      printFmt("(run68) watchpoint:MPUが命令0x%04xを実行しました。\n",
               cwatchpoint);
      if (stepcount != 0) {
        printFmt("(run68) breakpoint:%d counts left.\n", stepcount);
        stepcount = 0;
      }
    }
    if (debug_on) {
      debug_on = false;
      debug_flag = false;
      RUN68_COMMAND cmd = debugger(running);
      switch (cmd) {
        default:
          break;
        case RUN68_COMMAND_RUN:
          *restart = true;
          running = true;
          goto EndOfFunc;
        case RUN68_COMMAND_CONT:
          goto NextInstruction;
        case RUN68_COMMAND_STEP:
        case RUN68_COMMAND_NEXT:
          debug_on = true;
          goto NextInstruction;
        case RUN68_COMMAND_QUIT:
          cont_flag = false;
          goto NextInstruction;
      }
    } else if (stepcount != 0) {
      stepcount--;
      if (stepcount == 0) {
        debug_on = true;
      }
    }
    if ((pc & 0xFF000001) != 0) {
      err68b("アドレスエラーが発生しました", pc, OPBuf_getentry(0)->pc);
      break;
    }
  NextInstruction:
    /* PCの値とニーモニックを保存する */
    OP_info.pc = pc;
    OP_info.code = *((unsigned short *)(prog_ptr + pc));
    if (setjmp(jmp_when_abort) != 0) {
      debug_on = true;
      continue;
    }
    if (prog_exec()) {
      running = false;
      if (debug_flag) {
        debug_on = true;
      } else {
        cont_flag = false;
      }
    }
    OPBuf_insert(&OP_info);
  } while (cont_flag);
EndOfFunc:
  return rd[0];
}

/*
   機能：
     割り込みをエミュレートせずに実行する
   戻り値：
     終了コード
*/
static int exec_notrap(bool *restart) {
  static bool cont_flag = true;
  static bool running = true;

  *restart = false;
  OPBuf_clear();
  do {
    /* 実行した命令の情報を保存しておく */
    OP_info.pc = 0;
    OP_info.code = 0;
    OP_info.rmem = 0;
    OP_info.rsize = 'N';
    OP_info.wmem = 0;
    OP_info.wsize = 'N';
    OP_info.mnemonic[0] = 0;
    if (superjsr_ret == pc) {
      SR_S_OFF();
      superjsr_ret = 0;
    }
    if (trap_pc != 0 && pc == trap_pc) {
      printFmt("(run68) breakpoint:MPUがアドレス$%06Xの命令を実行しました。\n",
               pc);
      debug_on = true;
      if (stepcount != 0) {
        printFmt("(run68) breakpoint:%d counts left.\n", stepcount);
        stepcount = 0;
      }
    } else if (cwatchpoint != 0x4afc && cwatchpoint == PeekW(pc)) {
      printFmt("(run68) watchpoint:MPUが命令0x%04xを実行しました。\n",
               cwatchpoint);
      debug_on = true;
      if (stepcount != 0) {
        printFmt("(run68) breakpoint:%d counts left.\n", stepcount);
        stepcount = 0;
      }
    }
    if (debug_on) {
      debug_on = false;
      debug_flag = true;
      RUN68_COMMAND cmd = debugger(running);
      switch (cmd) {
        default:
          break;
        case RUN68_COMMAND_RUN:
          *restart = true;
          running = true;
          goto EndOfFunc;
        case RUN68_COMMAND_CONT:
          goto NextInstruction;
        case RUN68_COMMAND_STEP:
        case RUN68_COMMAND_NEXT:
          debug_on = true;
          goto NextInstruction;
        case RUN68_COMMAND_QUIT:
          cont_flag = false;
          goto NextInstruction;
      }
    } else if (stepcount != 0) {
      stepcount--;
      if (stepcount == 0) {
        debug_on = true;
      }
    }
    if ((pc & 0xFF000001) != 0) {
      err68b("アドレスエラーが発生しました", pc, OPBuf_getentry(0)->pc);
      break;
    }
  NextInstruction:
    /* PCの値とニーモニックを保存する */
    OP_info.pc = pc;
    OP_info.code = *((unsigned short *)(prog_ptr + pc));
    if (setjmp(jmp_when_abort) != 0) {
      debug_on = true;
      continue;
    }
    if (prog_exec()) {
      running = false;
      if (debug_flag) {
        debug_on = true;
      } else {
        cont_flag = false;
      }
    }
    OPBuf_insert(&OP_info);
  } while (cont_flag);
EndOfFunc:
  return rd[0];
}

static void init_fileinfo(int fileno, bool is_opened, short mode) {
  FILEINFO *finfop = &finfo[fileno];

  HOST_INIT_FILEINFO(finfop, fileno);
  finfop->is_opened = is_opened;
  finfop->date = 0;
  finfop->time = 0;
  finfop->mode = mode;
  finfop->nest = 0;
  finfop->name[0] = '\0';
}

// ファイル管理テーブルの初期化
static void init_all_fileinfo(void) {
  init_fileinfo(HUMAN68K_STDIN, true, 0);
  init_fileinfo(HUMAN68K_STDOUT, true, 1);
  init_fileinfo(HUMAN68K_STDERR, true, 1);

  int i;
  for (i = HUMAN68K_STDERR + 1; i < FILE_MAX; i++) {
    init_fileinfo(i, false, 0);
  }
}

static ULong malloc_for_child(ULong parent) {
  ULong size = Malloc(MALLOC_FROM_LOWER, (ULong)-1, parent) & 0x00ffffff;
  if (size < 256 * 1024) {
    return 0;
  }

  Long adr = Malloc(MALLOC_FROM_LOWER, size, parent);
  return (adr < 0) ? 0 : adr - SIZEOF_MEMBLK;
}

static ULong init_env(ULong size, ULong parent) {
  Long buf = Malloc(MALLOC_FROM_LOWER, size, parent);
  if (buf != 0) {
    WriteSuperULong(buf, size);
    WriteSuperUByte(buf + 4, 0);
    readenv_from_ini(ini_file_name, buf);
  }
  return buf;
}

static void setHuman68kPathName(Human68kPathName *hpn, const char *path,
                                const char *name, const char *ext) {
  strncpy(hpn->path, path, sizeof(hpn->path));
  strncpy(hpn->name, name, sizeof(hpn->name));
  strncat(hpn->name, ext, sizeof(hpn->name));
  hpn->nameLen = strlen(name);
  hpn->extLen = strlen(ext);
}

int main(int argc, char *argv[]) {
  char fname[89]; /* 実行ファイル名 */
  FILE *fp;       /* 実行ファイルのファイルポインタ */
  int i, j;
  bool restart;

  debug_flag = false;
Restart:
  /* コマンドライン解析 */
  for (i = 1; i < argc; i++) {
    /* フラグを調べる。 */
    if (argv[i][0] == '-') {
      bool invalid_flag = false;
      char *fsp = argv[i];
      switch (argv[i][1]) {
        case 't':
          if (strlen(argv[i]) == 2) {
            trace_f = true;
            print("MPU命令トレースフラグ=ON\n");
          } else if (argv[i][2] == 'r') {
            char *p; /* アドレス文字列へのポインタ */
            if (strlen(argv[i]) == strlen("-tr")) {
              i++; /* "-tr"とアドレスとの間に空白あり。*/
              p = argv[i];
            } else {
              p = &(argv[i][3]);
            }
            /* 16進文字列が正しいことを確認 */
            for (j = 0; (unsigned int)j < strlen(p); j++) {
              if (!isxdigit((unsigned char)p[j])) {
                printFmt("16進アドレス指定は無効です。(\"%s\")\n", p);
                invalid_flag = true;
              }
            }
            /* トラップするPCのアドレスを取得する。*/
            sscanf(p, "%x", &trap_pc);
            printFmt("MPU命令トラップフラグ=ON ADDR=$%06X\n", trap_pc);
          } else {
            invalid_flag = true;
          }
          break;
        case 'd':
          if (strcmp(argv[i], "-debug") != 0) {
            invalid_flag = true;
            break;
          }
          debug_on = true;
          debug_flag = false;
          print("デバッガを起動します。\n");
          break;
        case 'f':
          func_trace_f = true;
          print("ファンクションコールトレースフラグ=ON\n");
          break;
        default:
          invalid_flag = true;
          break;
      }
      if (invalid_flag) printFmt("無効なフラグ'%s'は無視されます。\n", fsp);
    } else {
      break;
    }
  }

  int argbase = i;  // アプリケーションコマンドラインの開始位置
  if (argc - argbase == 0) {
    print_title();
    print_usage();
    return EXIT_FAILURE;
  }

  /* iniファイルの情報を読み込む */
  strcpy(ini_file_name, argv[0]);
  /* iniファイルのフルパス名が得られる。*/
  read_ini(ini_file_name);

  /* メインメモリ(1～12MB)を確保する */
  if ((prog_ptr = malloc(mem_aloc)) == NULL) {
    printFmt("メモリが確保できません\n");
    return EXIT_FAILURE;
  }
  memset(prog_ptr, 0, HUMAN_TAIL);
  WriteSuperULong(OSWORK_MEMORY_END, mem_aloc);

  trap_table_make();

  // Human68kのPSPを作成
  const ULong humanPsp = HUMAN_HEAD;
  const ULong humanCodeSize = HUMAN_TAIL - (humanPsp + SIZEOF_PSP);
  BuildMemoryBlock(humanPsp, 0, 0, HUMAN_TAIL, 0);
  const ProgramSpec humanSpec = {humanCodeSize, 0};
  const Human68kPathName humanName = {"A:\\", "HUMAN.SYS", 0, 0};
  BuildPsp(humanPsp, -1, 0, 0x2000, humanPsp, &humanSpec, &humanName);
  WriteSuperULong(OSWORK_ROOT_PSP, humanPsp);
  nest_cnt = 0;

  WriteDrviceHeaders();

  // メインメモリの0番地からHuman68kの末尾までをスーパーバイザ領域に設定する。
  // エリアセットレジスタの仕様上は8KB単位。
  // Human68kの使用メモリを動的に変更する場合は、メモリブロックの末尾アドレスも
  // 同期させること。
  SetSupervisorArea(HUMAN_TAIL);

  // 環境変数を初期化
  const ULong humanEnv = init_env(DEFAULT_ENV_SIZE, humanPsp);

  /* 実行ファイルのオープン */
  if (strlen(argv[argbase]) >= sizeof(fname)) {
    print("ファイルのパス名が長すぎます\n");
    return EXIT_FAILURE;
  }
  strcpy(fname, argv[argbase]);
  /*
   * プログラムをPATH環境変数で設定したディレクトリから探して
   * 読み込みを行う。
   */
  if ((fp = prog_open(fname, humanEnv, print)) == NULL) {
    printFmt("run68:Program '%s' was not found.\n", argv[argbase]);
    return EXIT_FAILURE;
  }

  Human68kPathName hpn;
  if (!HOST_CANONICAL_PATHNAME(fname, &hpn)) {
    setHuman68kPathName(&hpn, "A:\\", "PROG", ".X");
    printFmt(
        "run68:Human68k形式のパス名に変換できないため、PSP内の実行ファイル名を"
        "\"%s%s\"に変更します。\n",
        hpn.path, hpn.name);
  }

  // コマンドライン文字列を作成
  bool needHupair;
  ULong cmdline = EncodeHupair(argc - (argbase + 1), &argv[argbase + 1],
                               hpn.name, humanPsp, &needHupair);

  // スタックを確保
  const ULong programStack =
      Malloc(MALLOC_FROM_LOWER, DEFAULT_STACK_SIZE, humanPsp);
  const ULong stackBottom = programStack + DEFAULT_STACK_SIZE;

  const ULong programPsp = malloc_for_child(humanPsp);

  if (humanEnv == 0 || cmdline == 0 || programStack == 0 || programPsp == 0) {
    print("メインメモリからプロセス用のメモリを確保できません\n");
    return EXIT_FAILURE;
  }

  /* プログラムをメモリに読み込む */
  Long prog_size = 0;          // プログラムサイズ(bss含む)
  Long prog_size2 = mem_aloc;  // プログラムサイズ(bss除く)
  const Long entryAddress = prog_read(fp, fname, programPsp + SIZEOF_PSP,
                                      &prog_size, &prog_size2, print);
  if (entryAddress < 0) {
    free(prog_ptr);
    return EXIT_FAILURE;
  }

  if (needHupair) {
    if (!IsCompliantWithHupair(programPsp + SIZEOF_PSP, prog_size,
                               entryAddress)) {
      print(
          "コマンドライン文字列の長さが255バイトを超えましたが、"
          "プログラムがHUPAIRに対応していないため実行できません。\n");
      return EXIT_FAILURE;
    }
  }

  const ProgramSpec progSpec = {prog_size2, prog_size - prog_size2};
  BuildPsp(programPsp, humanEnv, cmdline, sr, humanPsp, &progSpec, &hpn);

  init_all_fileinfo();

  /* レジスタに値を設定 */
  pc = entryAddress;
  ra[0] = programPsp;
  ra[1] =
      programPsp + SIZEOF_PSP + prog_size;  // プログラムの終わり+1のアドレス
  ra[2] = cmdline;   // コマンドラインのアドレス
  ra[3] = humanEnv;  // 環境のアドレス
  ra[4] = pc;        // 実行開始アドレス
  ra[7] = stackBottom;

  /* 実行 */
  psp[nest_cnt] = programPsp;
  superjsr_ret = 0;
  usp = 0;
  int ret = ini_info.trap_emulate ? exec_trap(&restart) : exec_notrap(&restart);

  /* 終了 */
  if (trace_f || func_trace_f) {
    printf("d0-7=%08x", rd[0]);
    for (i = 1; i < 8; i++) {
      printf(",%08x", rd[i]);
    }
    printf("\n");
    printf("a0-7=%08x", ra[0]);
    for (i = 1; i < 8; i++) {
      printf(",%08x", ra[i]);
    }
    printf("\n");
    printf("  pc=%08x    sr=%04x\n", pc, sr);
  }

  free(prog_ptr);
  prog_ptr = NULL;

  if (restart) goto Restart;
  return ret;
}

// 標準エラー出力に文字列を出力する
void print(const char *message) { fputs(message, stderr); }

// 標準エラー出力にフォーマット文字列を出力する
void printFmt(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

/* $Id: run68.c,v 1.5 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
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
 * Revision 1.2  2004/12/16 12:25:11  masamic
 * It has become under GPL.
 * Maintenor name has changed.
 * Modify codes for aboves.
 *
 * Revision 1.1.1.1  2001/05/23 11:22:08  masamic
 * First imported source code and docs
 *
 * Revision 1.15  2000/01/16  05:38:16  yfujii
 * 'Program not found' message is changed.
 *
 * Revision 1.14  1999/12/23  08:06:27  yfujii
 * Bugs of FILES/NFILES calls are fixed.
 *
 * Revision 1.13  1999/12/07  12:47:40  yfujii
 * *** empty log message ***
 *
 * Revision 1.13  1999/12/01  13:53:48  yfujii
 * Help messages are modified.
 *
 * Revision 1.12  1999/12/01  04:02:55  yfujii
 * .ini file is now retrieved from the same dir as the run68.exe file.
 *
 * Revision 1.11  1999/11/29  06:19:00  yfujii
 * PATH is enabled when loading executable.
 *
 * Revision 1.10  1999/11/01  06:23:33  yfujii
 * Some debugging functions are introduced.
 *
 * Revision 1.9  1999/10/29  13:44:04  yfujii
 * Debugging facilities are introduced.
 *
 * Revision 1.8  1999/10/26  12:26:08  yfujii
 * Environment variable function is drasticaly modified.
 *
 * Revision 1.7  1999/10/26  01:31:54  yfujii
 * Execution history and address trap is added.
 *
 * Revision 1.6  1999/10/25  03:25:33  yfujii
 * Command options are added (-f -t).
 *
 * Revision 1.5  1999/10/22  03:44:30  yfujii
 * Re-trying the same modification failed last time.
 *
 * Revision 1.3  1999/10/21  12:03:19  yfujii
 * An preprocessor directive error is removed.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
