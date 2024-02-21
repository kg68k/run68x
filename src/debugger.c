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

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "mem.h"
#include "run68.h"

/* デバッグモードのプロンプト */
#define PROMPT "(run68)"
/* コマンドラインの最大文字列長 */
#define MAX_LINE 256

static char *command_name[] = {
    "BREAK",   /* ブレークポイントの設定 */
    "CLEAR",   /* ブレークポイントの解除 */
    "CONT",    /* 実行の継続 */
    "DUMP",    /* メモリをダンプする */
    "HELP",    /* 命令の実行履歴 */
    "HISTORY", /* 命令の実行履歴 */
    "LIST",    /* ディスアセンブル */
    "NEXT", /* STEPと同じ。ただし、サブルーチン呼出しはスキップ */
    "QUIT",  /* run68を終了する */
    "REG",   /* レジスタの内容を表示する */
    "RUN",   /* 環境を初期化してプログラム実行 */
    "SET",   /* メモリに値をセットする */
    "STEP",  /* 一命令分ステップ実行 */
    "WATCHC" /* 命令ウォッチ */
};

ULong stepcount;

static RUN68_COMMAND analyze(const char *line, int *argc, char **argv);
static void display_help();
static void display_history(int argc, char **argv);
static void display_list(int argc, char **argv);
static void run68_dump(int argc, char **argv);
static void display_registers();
static void set_breakpoint(int argc, char **argv);
static void clear_breakpoint();
static ULong get_stepcount(int argc, char **argv);
static void debuggerError(const char *fmt, ...) GCC_FORMAT(1, 2);

typedef enum {
  ARGUMENT_TYPE_NAME = 0,
  ARGUMENT_TYPE_DECIMAL = 1,
  ARGUMENT_TYPE_HEX = 2,
  ARGUMENT_TYPE_SYMBOL = 3,
} ArgumentType;

// 文字列が名前か、10進数値か、16進数か、あるいは記号かを判定する。
static ArgumentType determine_string(const char *str) {
  // とりあえずいい加減な実装をする。
  if (isupper(str[0])) {
    return ARGUMENT_TYPE_NAME;  // 名前
  } else if (isdigit(str[0])) {
    return ARGUMENT_TYPE_DECIMAL;  // 10進数
  } else if (str[0] == '$') {
    return ARGUMENT_TYPE_HEX;  // 16進数
  }
  return ARGUMENT_TYPE_SYMBOL;  // 記号
}

static UWord watchcode(int argc, char **argv) {
  unsigned short wcode;

  if (argc < 2) {
    debuggerError("watchcode:Too few arguments.\n");
    return 0x4afc;
  } else if (determine_string(argv[1]) != 2) {
    debuggerError("watchcode:Instruction code expression error.\n");
    return 0x4afc;
  }
  sscanf(&argv[1][1], "%hx", &wcode);
  return (UWord)wcode;
}

/*
   機能：
     run68をデバッグモードで起動すると、この関数が呼出される。
   パラメータ：
     bool running  - アプリケーションプログラムの実行中はtrueで
                     呼出される。
   戻り値：
     COMMAND - 呼び側のコードで実行すべきコマンドを表している。
*/
RUN68_COMMAND debugger(bool running) {
  RUN68_COMMAND cmd;

  if (running) {
    Long naddr, addr = pc;
    char hex[64];
    char *s = disassemble(addr, &naddr);
    int j;

    /* まず全レジスタを表示し、*/
    display_registers();
    /* 1命令分、逆アセンブルして表示する。*/
    sprintf(hex, "$%06X ", addr);
    if (addr == naddr) {
      /* ディスアセンブルできなかった */
      naddr += 2;
    }
    while (addr < naddr) {
      char *p = hex + strlen(hex);
      sprintf(p, "%04X ", PeekW(addr));
      addr += 2;
    }
    for (j = strlen(hex); j < 34; j++) {
      hex[j] = ' ';
    }
    hex[j] = '\0';
    printFmt("%s%s\n", hex, s ? s : "\?\?\?\?");
  } else {
    stepcount = 0;
  }
  if (stepcount != 0) {
    stepcount--;
    cmd = RUN68_COMMAND_STEP;
    goto EndOfLoop;
  }
  /* コマンドループ */
  while (true) {
    char line[MAX_LINE];
    char *argv[MAX_LINE];
    int argc;
    print(PROMPT);
    if (fgets(line, MAX_LINE, stdin) == NULL) {
      if (feof(stdin) || ferror(stdin)) {
        print("quit\n");
        cmd = RUN68_COMMAND_QUIT;
        goto EndOfLoop;
      }
    }
    cmd = analyze(line, &argc, argv);
    if (argc == 0) {
      continue;
    }
    switch (cmd) {
      case RUN68_COMMAND_BREAK: /* ブレークポイントの設定 */
        set_breakpoint(argc, argv);
        break;
      case RUN68_COMMAND_CLEAR: /* ブレークポイントの解除 */
        clear_breakpoint();
        break;
      case RUN68_COMMAND_CONT: /* 実行の継続 */
        if (!running) {
          print("Program is not running!\n");
          break;
        }
        stepcount = get_stepcount(argc, argv);
        goto EndOfLoop;
      case RUN68_COMMAND_DUMP: /* メモリをダンプする */
        run68_dump(argc, argv);
        break;
      case RUN68_COMMAND_HELP: /* デバッガのヘルプ */
        display_help();
        break;
      case RUN68_COMMAND_HISTORY: /* 命令の実行履歴 */
        display_history(argc, argv);
        break;
      case RUN68_COMMAND_LIST: /* ディスアセンブル */
        display_list(argc, argv);
        break;
      case RUN68_COMMAND_NEXT: /* STEPと同じ。ただし、サブルーチン呼出しはスキップ
                                */
        if (!running) {
          print("Program is not running!\n");
          break;
        }
        goto EndOfLoop;
      case RUN68_COMMAND_QUIT: /* run68を終了する */
        goto EndOfLoop;
      case RUN68_COMMAND_REG: /* レジスタの値を表示する */
        display_registers();
        break;
      case RUN68_COMMAND_RUN: /* 環境を初期化してプログラム実行 */
        goto EndOfLoop;
      case RUN68_COMMAND_SET: /* メモリに値をセットする */
        printFmt("cmd:%s is not implemented yet.\n", argv[0]);
        break;
      case RUN68_COMMAND_STEP: /* 一命令分ステップ実行 */
        if (!running) {
          print("Program is not running!\n");
          break;
        }
        stepcount = get_stepcount(argc, argv);
        goto EndOfLoop;
      case RUN68_COMMAND_WATCHC: /* 命令ウォッチ */
        cwatchpoint = watchcode(argc, argv);
        break;
      case RUN68_COMMAND_NULL: /* コマンドではない(移動禁止) */
        printFmt("cmd:%s is not a command.\n", argv[0]);
        break;
      case RUN68_COMMAND_ERROR: /* コマンドエラー(移動禁止) */
        printFmt("Command line error:\"%s\"\n", argv[0]);
        break;
    }
  }
EndOfLoop:
  return cmd;
}

/*
   機能：
     コマンドライン文字列を解析し、コマンドとその引き数を取り出す。
   パラメータ：
     const char* line  <in>  コマンドライン文字列
     int*        argc  <out> コマンドラインに含まれるトークン数
     char**      argv  <out> トークンに分解された文字列の配列
   戻り値：
     COMMAND コマンドの列挙値
 */
static RUN68_COMMAND analyze(const char *line, int *argc, char **argv) {
  static char cline[MAX_LINE * 2];
  unsigned int i;
  char *q = cline;

  *argc = 0;
  for (i = 0; i < strlen(line); i++) {
    /* 空白文字を読み飛ばす。*/
    const char *p = &line[i];
    char c = toupper(*p++);
    if (c == ' ' || c == '\t') {
      continue;
    } else if ('A' <= c && c <= 'Z') {
      /* コマンド等の名前 */
      argv[(*argc)++] = q;
      do {
        *q++ = c;
        c = toupper(*p++);
      } while (('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '_');
      *q++ = '\0';
      i += strlen(argv[*argc - 1]);
    } else if ('0' <= c && c <= '9') {
      /* 10進数 */
      argv[(*argc)++] = q;
      do {
        *(q++) = c;
        c = toupper(*p++);
      } while ('0' <= c && c <= '9');
      *q++ = '\0';
      i += strlen(argv[*argc - 1]);
    } else if ((c == '$' && 'A' <= toupper(*p) && toupper(*p) <= 'F') ||
               ('0' <= *p && *p <= '9')) {
      /* 16進数は$記号を付ける。*/
      argv[(*argc)++] = q;
      *q++ = c;
      c = toupper(*p++);
      do {
        *q++ = c;
        c = toupper(*p++);
      } while (('A' <= c && c <= 'F') || ('0' <= c && c <= '9'));
      *q++ = '\0';
      i += strlen(argv[*argc - 1]);
    }
  }
  if (*argc == 0) {
    return RUN68_COMMAND_NULL;
  } else if ('A' <= argv[0][0] && argv[0][0] <= 'Z') {
    RUN68_COMMAND cmd;
    for (cmd = (RUN68_COMMAND)0; cmd < RUN68_COMMAND_NULL; cmd++) {
      if (strcmp(argv[0], command_name[cmd]) == 0) {
        return cmd;
      }
    }
    return RUN68_COMMAND_ERROR;
  } else {
    return RUN68_COMMAND_ERROR;
  }
}

static void display_help() {
  const char *help =
      "       ============= run68 debugger commands =============\n"
      "break $adr      - Set a breakpoint.\n"
      "clear           - Clear the breakpoint.\n"
      "cont            - Continue running.\n"
      "cont n          - Continue running and stops after executing n "
      "instructions.\n"
      "dump $adr [n]   - Dump memory (n bytes) from $adr.\n"
      "dump [n]        - Dump memory (n bytes) continuously.\n"
      "help            - Show this menu.\n"
      "history [n]     - Show last n instructions executed.\n"
      "list $adr [n]   - Disassemble from $adr.\n"
      "list [n]        - Disassemble n instructions from current PC.\n"
      "quit            - Quit from run68.\n"
      "reg             - Display registers.\n"
      "run             - Run Human68k program from the begining.\n"
      "step            - Execute only one instruction.\n"
      "step n          - Continue running with showing all registers\n"
      "                  and stops after executing n instructions.\n";
  print(help);
}

static bool scanSize(const char *s, int *sizeptr) {
  int size;
  if (sscanf(s, "%d", &size) != 1 || size <= 0 || size > 1024) return false;

  *sizeptr = size;
  return true;
}

static bool scanAddress(const char *s, ULong *adrptr) {
  int adr;
  const char *afterDoller = s + 1;
  if (sscanf(afterDoller, "%x", &adr) != 1) return false;

  *adrptr = adr & ADDRESS_MASK;
  return true;
}

static void run68_dump(int argc, char **argv) {
  static ULong dump_addr = 0;
  static int size = 32;
  int i, j;

  bool argumentError = false;
  if (argc >= 2) {
    switch (determine_string(argv[1])) {
      default:
        argumentError = true;
        break;
      case ARGUMENT_TYPE_DECIMAL:
        if (!scanSize(argv[1], &size)) argumentError = true;
        break;
      case ARGUMENT_TYPE_HEX:
        if (!scanAddress(argv[1], &dump_addr)) argumentError = true;
        break;
    }
  }
  if (argc >= 3) {
    switch (determine_string(argv[2])) {
      default:
        argumentError = true;
        break;
      case ARGUMENT_TYPE_DECIMAL:
        if (!scanSize(argv[2], &size)) argumentError = true;
        break;
    }
  }
  if (argumentError) {
    debuggerError("dump:Argument error.\n");
    return;
  }

  MemoryRange mem;
  if (!GetWritableMemoryRangeSuper(dump_addr, size, &mem) || (int)mem.length < size) {
    // ダンプ対象がメインメモリ外ならエラー
    debuggerError("dump:Address range error.\n");
    return;
  }

  for (i = 0; i < size; i++) {
    ULong d = mem.bufptr[i];
    if (i % 16 == 0) {
      printFmt("%06X:", dump_addr + i);
    } else {
      print((i % 8 == 0) ? "-" : " ");
    }
    printFmt("%02X", d);
    if (i % 16 == 15 || i == size - 1) {
      if (i % 16 != 15) {
        for (j = i % 16 + 1; j < 16; j++) {
          print("   ");
        }
      }
      print(":");
      for (j = i & 0xfffffff0; j <= i; j++) {
        d = mem.bufptr[j];
        printFmt("%c", (' ' <= d && d <= 0x7e) ? d : '.');
      }
      print("\n");
    }
  }
  dump_addr += size;
}

static void display_registers() {
  int i;
  printFmt("D0-D7=%08X", rd[0]);
  for (i = 1; i < 8; i++) {
    printFmt(",%08X", rd[i]);
  }
  print("\n");
  printFmt("A0-A7=%08X", ra[0]);
  for (i = 1; i < 8; i++) {
    printFmt(",%08X", ra[i]);
  }
  print("\n");
  printFmt("  PC=%08X    SR=%04X\n", pc, sr);
}

static void set_breakpoint(int argc, char **argv) {
  if (argc < 2) {
    if (trap_pc == 0) {
      debuggerError("break:No breakpoints set.\n");
    } else {
      debuggerError("break:Breakpoint is set to $%06X.\n", trap_pc);
    }
    return;
  } else if (determine_string(argv[1]) != 2) {
    debuggerError("break:Address expression error.\n");
    return;
  }

  {
    long unsigned int a;
    sscanf(&argv[1][1], "%lx", &a);
    trap_pc = (Long)a;
  }
}

static void clear_breakpoint() { trap_pc = 0; }

static void display_history(int argc, char **argv) {
  int n = 0;
  if (argc == 1) {
    n = 10;
  } else if (determine_string(argv[1]) != 1) {
    debuggerError("history:Argument error.\n");
  } else {
    sscanf(argv[1], "%d", &n);
  }
  OPBuf_display(n);
}

static void display_list(int argc, char **argv) {
  static Long list_addr = 0;
  static Long old_pc = 0;
  Long addr, naddr;
  int i, j, n;

  n = 10;
  if (old_pc == 0) {
    old_pc = pc;
  } else if (old_pc != pc) {
    old_pc = pc;
    list_addr = 0;
  }
  if (list_addr == 0) {
    addr = pc;
  } else {
    addr = list_addr;
  }
  if (2 <= argc) {
    if (argc == 2 && determine_string(argv[1]) == 1) {
      sscanf(argv[1], "%d", &n);
    } else if (argc >= 2 && determine_string(argv[1]) == 2) {
      sscanf(&argv[1][1], "%x", &addr);
    }
    if (argc == 3 && determine_string(argv[2]) == 1) {
      sscanf(argv[2], "%d", &n);
    }
  }
  for (i = 0; i < n; i++) {
    char *s = disassemble(addr, &naddr);
    char hex[64];

    sprintf(hex, "$%06X ", addr);
    if (addr == naddr) {
      /* ディスアセンブルできなかった */
      naddr += 2;
    }
    while (addr < naddr) {
      char *p = hex + strlen(hex);
      sprintf(p, "%04X ", PeekW(addr));
      addr += 2;
    }
    for (j = strlen(hex); j < 34; j++) {
      hex[j] = ' ';
    }
    hex[j] = '\0';
    printFmt("%s%s\n", hex, s ? s : "\?\?\?\?");
  }
  list_addr = naddr;
}

static ULong get_stepcount(int argc, char **argv) {
  ULong count = 0;
  if (argc == 1) {
    return 0;
  } else if (determine_string(argv[1]) == 1) {
    long unsigned int a;
    sscanf(argv[1], "%lu", &a);
    count = (ULong)a;
  }
  return count;
}

// エラー表示
static void debuggerError(const char *fmt, ...) {
  va_list ap;

  fputs("run68-", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

/* $Id: debugger.c,v 1.2 2009-08-08 06:49:44 masamic Exp $*/

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:06  masamic
 * First imported source code and docs
 *
 * Revision 1.5  1999/12/23  08:07:58  yfujii
 * Help messages are changed.
 *
 * Revision 1.4  1999/12/07  12:40:12  yfujii
 * *** empty log message ***
 *
 * Revision 1.4  1999/12/01  13:53:48  yfujii
 * Help messages are modified.
 *
 * Revision 1.3  1999/11/29  06:16:47  yfujii
 * Disassemble and step command are implemented.
 *
 * Revision 1.2  1999/11/01  06:23:33  yfujii
 * Some debugging functions are introduced.
 *
 * Revision 1.1  1999/10/29  13:41:07  yfujii
 * Initial revision
 *
 */
