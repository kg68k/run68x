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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define prog_ptr_u ((unsigned char *)prog_ptr)

EXEC_INSTRUCTION_INFO OP_info;
FILEINFO finfo[FILE_MAX];
INI_INFO ini_info;
const char size_char[3] = {'b', 'w', 'l'};
Long ra[8];
Long rd[8 + 1];
Long usp;
Long pc;
short sr;
char *prog_ptr;
int trap_count;
Long superjsr_ret;
Long psp[NEST_MAX];
Long nest_pc[NEST_MAX];
Long nest_sp[NEST_MAX];
unsigned int nest_cnt;
jmp_buf jmp_when_abort;
Long mem_aloc;
bool func_trace_f = false;
Long trap_pc;
unsigned short cwatchpoint = 0x4afc;

static bool trace_f = false;
static bool debug_on = false;
static bool debug_flag = false;

static char ini_file_name[MAX_PATH];

#ifndef _WIN32
char *strlwr(char *str) {
  unsigned char *p = (unsigned char *)str;

  while (*p) {
    *p = tolower((unsigned char)*p);
    p++;
  }

  return str;
}
#endif

static void print_title(void) {
  const char *title =  //
      "run68x " RUN68X_VERSION
      "  Copyright (C) 2023 TcbnErik\n"
      "  based on "
      "X68000 console emulator Ver." RUN68VERSION "\n";
  fprintf(stderr, "%s", title);
  fprintf(stderr, "          %s\n", "Created in 1996 by Yokko");
  fprintf(stderr, "          %s\n",
          "Maintained since Oct. 1999 by masamic and Chack'n");
}

static void print_usage(void) {
  const char *usage =
      "Usage: run68 [options] execute_filename [commandline]\n"
      "             -f         function call trace\n"
      "             -t         mpu trace\n"
      "             -debug     run with debugger\n";
  fprintf(stderr, "%s", usage);
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

/*
 　機能：割り込みをエミュレートして実行する
 戻り値：なし
*/
static int exec_trap(bool *restart) {
  UChar *trap_mem1;
  UChar *trap_mem2;
  Long trap_adr;
  static bool cont_flag = true;
  static bool running = true;

  trap_count = 1;
  trap_mem1 = (UChar *)prog_ptr + 0x118;
  trap_mem2 = (UChar *)prog_ptr + 0x138;
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
      fprintf(stderr,
              "(run68) trapped:MPUがアドレス$%06Xの命令を実行しました。\n", pc);
      debug_on = true;
      if (stepcount != 0) {
        fprintf(stderr, "(run68) breakpoint:%d counts left.\n", stepcount);
        stepcount = 0;
      }
    } else if (cwatchpoint != 0x4afc &&
               cwatchpoint == *((unsigned short *)(prog_ptr + pc))) {
      fprintf(stderr, "(run68) watchpoint:MPUが命令0x%04xを実行しました。\n",
              cwatchpoint);
      if (stepcount != 0) {
        fprintf(stderr, "(run68) breakpoint:%d counts left.\n", stepcount);
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
      fprintf(stderr,
              "(run68) breakpoint:MPUがアドレス$%06Xの命令を実行しました。\n",
              pc);
      debug_on = true;
      if (stepcount != 0) {
        fprintf(stderr, "(run68) breakpoint:%d counts left.\n", stepcount);
        stepcount = 0;
      }
    } else if (cwatchpoint != 0x4afc &&
               cwatchpoint == ((unsigned short)prog_ptr_u[pc] << 8) +
                                  (unsigned short)prog_ptr_u[pc + 1]) {
      fprintf(stderr, "(run68) watchpoint:MPUが命令0x%04xを実行しました。\n",
              cwatchpoint);
      debug_on = true;
      if (stepcount != 0) {
        fprintf(stderr, "(run68) breakpoint:%d counts left.\n", stepcount);
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

int main(int argc, char *argv[], char *envp[]) {
  char fname[89]; /* 実行ファイル名 */
  FILE *fp;       /* 実行ファイルのファイルポインタ */
  char *arg_ptr;  /* コマンドライン文字列格納領域 */
#if !defined(ENV_FROM_INI)
  char buf[ENV_SIZE];
  char *mem_ptr; /* メモリ管理ブロック */
  char *read_ptr;
  int env_len = 0; /* 環境の長さ */
#endif
  Long prog_size;  /* プログラムサイズ(bss含む) */
  Long prog_size2; /* プログラムサイズ(bss除く) */
  int arg_len = 0; /* コマンドラインの長さ */
  int i, j;
  int argbase = 0; /* アプリケーションコマンドラインの開始位置 */
  int ret;
  bool restart;

  debug_flag = false;
Restart:
  arg_len = 0;
  argbase = 0;
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
            fprintf(stderr, "MPU命令トレースフラグ=ON\n");
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
                fprintf(stderr, "16進アドレス指定は無効です。(\"%s\")\n", p);
                invalid_flag = true;
              }
            }
            /* トラップするPCのアドレスを取得する。*/
            sscanf(p, "%x", &trap_pc);
            fprintf(stderr, "MPU命令トラップフラグ=ON ADDR=$%06X\n", trap_pc);
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
          fprintf(stderr, "デバッガを起動します。\n");
          break;
        case 'f':
          func_trace_f = true;
          fprintf(stderr, "ファンクションコールトレースフラグ=ON\n");
          break;
        default:
          invalid_flag = true;
          break;
      }
      if (invalid_flag)
        fprintf(stderr, "無効なフラグ'%s'は無視されます。\n", fsp);
    } else {
      break;
    }
  }
  argbase = i; /* argbase以前の引数はすべてオプションである。*/
  if (argc - argbase == 0) {
    print_title();
    print_usage();
    return EXIT_FAILURE;
  }

  /* iniファイルの情報を読み込む */
  strcpy(ini_file_name, argv[0]);
  /* iniファイルのフルパス名が得られる。*/
  read_ini(ini_file_name, fname);

  /* メモリを確保する */
  if ((prog_ptr = malloc(mem_aloc)) == NULL) {
    fprintf(stderr, "メモリが確保できません\n");
    return EXIT_FAILURE;
  }
  /* A0,A2,A3レジスタに値を設定 */
  ra[0] = STACK_TOP + STACK_SIZE; /* メモリ管理ブロックのアドレス */
  ra[2] = STACK_TOP;              /* コマンドラインのアドレス */
  ra[3] = ENV_TOP;                /* 環境のアドレス */

  /* 環境の設定 */
  mem_set(ra[3], ENV_SIZE, S_LONG);
  mem_set(ra[3] + 4, 0, S_BYTE);
#if defined(ENV_FROM_INI)
  /* 環境変数はiniファイルに記述する。(getini.c参照) */
  readenv_from_ini(ini_file_name);
#else
  /* Windowsの環境変数を複製する。*/
  for (i = 0; envp[i] != NULL; i++) {
    if (env_len + strlen(envp[i]) < ENV_SIZE - 5) {
      mem_ptr = prog_ptr + ra[3] + 4 + env_len;
      strcpy(mem_ptr, envp[i]);
      if (ini_info.env_lower) {
        strcpy(buf, envp[i]);
        strlwr(buf);
        read_ptr = buf;
        while (*mem_ptr != '\0' && *mem_ptr != '=')
          *(mem_ptr++) = *(read_ptr++);
      }
#ifdef TRACE
      mem_ptr = prog_ptr + ra[3] + 4 + env_len;
      printf("env: %s\n", mem_ptr);
#endif
      env_len += strlen(envp[i]) + 1;
    }
  }
  mem_set(ra[3] + 4 + env_len, 0, S_BYTE);
#endif
  /* 実行ファイルのオープン */
  if (strlen(argv[argbase]) >= sizeof(fname)) {
    fprintf(stderr, "ファイルのパス名が長すぎます\n");
    return EXIT_FAILURE;
  }
  strcpy(fname, argv[argbase]);
  /*
   * プログラムをPATH環境変数で設定したディレクトリから探して
   * 読み込みを行う。
   */
  if ((fp = prog_open(fname, true)) == NULL) {
    fprintf(stderr, "run68:Program '%s' was not found.\n", argv[argbase]);
    return EXIT_FAILURE;
  }

  /* プログラムをメモリに読み込む */
  prog_size2 = mem_aloc;
  pc = prog_read(fp, fname, PROG_TOP, &prog_size, &prog_size2, true);
  if (pc < 0) {
    free((void *)prog_ptr);
    return EXIT_FAILURE;
  }

  /* A1,A4レジスタに値を設定 */
  ra[1] = PROG_TOP + prog_size; /* プログラムの終わり+1のアドレス */
  ra[4] = pc;                   /* 実行開始アドレス */
  nest_cnt = 0;

  /* コマンドライン文字列設定 */
  arg_ptr = prog_ptr + ra[2];
  for (i = argbase + 1; i < argc; i++) {
    if (i > 2) {
      arg_len++;
      *(arg_ptr + arg_len) = ' ';
    }
    strcpy(arg_ptr + arg_len + 1, argv[i]);
    arg_len += strlen(argv[i]);
  }
  if (arg_len > 255) arg_len = 255;
  *arg_ptr = arg_len;
  *(arg_ptr + arg_len + 1) = 0x00;
#ifdef TRACE
  if (arg_len > 0) printf("command line: %s\n", arg_ptr + 1);
#endif

  /* Humanのメモリ管理ブロック設定 */
  SR_S_ON();
  mem_set(HUMAN_HEAD, 0, S_LONG);
  mem_set(HUMAN_HEAD + 0x04, 0, S_LONG);
  mem_set(HUMAN_HEAD + 0x08, HUMAN_WORK, S_LONG);
  mem_set(HUMAN_HEAD + 0x0C, ra[0], S_LONG);
  SR_S_OFF();

  /* プロセス管理ブロック設定 */
  if (!make_psp(fname, HUMAN_HEAD, mem_aloc, HUMAN_HEAD, prog_size2)) {
    free((void *)prog_ptr);
    fprintf(stderr, "実行ファイル名が長すぎます\n");
    return EXIT_FAILURE;
  }

  /* ファイル管理テーブルの初期化 */
  for (i = 0; i < FILE_MAX; i++) {
#ifdef _WIN32
    finfo[i].handle = NULL;
#else
    finfo[i].fp = NULL;
#endif
    finfo[i].date = 0;
    finfo[i].time = 0;
    finfo[i].mode = 0;
    finfo[i].nest = 0;
    finfo[i].name[0] = '\0';
  }
#ifdef _WIN32
  finfo[0].handle = GetStdHandle(STD_INPUT_HANDLE);
  finfo[1].handle = GetStdHandle(STD_OUTPUT_HANDLE);
  finfo[2].handle = GetStdHandle(STD_ERROR_HANDLE);
#else
  finfo[0].fp = stdin;
  finfo[1].fp = stdout;
  finfo[2].fp = stderr;
#endif
  finfo[0].mode = 0;
  finfo[1].mode = 1;
  finfo[2].mode = 1;
  trap_table_make();

  /* 実行 */
  ra[7] = STACK_TOP + STACK_SIZE;
  superjsr_ret = 0;
  usp = 0;
  if (ini_info.trap_emulate)
    ret = exec_trap(&restart);
  else
    ret = exec_notrap(&restart);

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
