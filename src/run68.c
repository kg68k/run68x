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
#include "operate.h"

EXEC_INSTRUCTION_INFO OP_info;
FILEINFO finfo[FILE_MAX];
const char size_char[3] = {'b', 'w', 'l'};
Long ra[8];
Long rd[8];
Long usp;
Long pc;
UWord sr;
Long superjsr_ret;
Long psp[NEST_MAX];
Long nest_pc[NEST_MAX];
Long nest_sp[NEST_MAX];
unsigned int nest_cnt;
jmp_buf jmp_when_abort;
UWord cwatchpoint = 0x4afc;

static bool debug_flag = false;

static char ini_file_name[MAX_PATH];

Settings settings;
static const Settings defaultSettings = {
    DEFAULT_MAIN_MEMORY_SIZE,  // mainMemorySize
    DEFAULT_HIGH_MEMORY_SIZE,  // highMemorySize

    0,      // trapPc
    false,  // traceFunc
    false,  // debug

    false  // iothrough
};

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
      "  -himem=<mb>  allocate high memory"
      "  -f           function call trace\n"
      "  -tr <adr>    mpu instruction trap\n"
      "  -debug       run with debugger\n";
  print(usage);
}

typedef struct {
  ULong vector;
  ULong handler;
} VectorSet;

/*
   機能：割り込みベクタテーブルを作成する
 戻り値：なし
*/
static void trap_table_make(void) {
  enum {
    rte = HUMAN_WORK,
    rts = HUMAN_WORK + 2,
  };
  static const VectorSet vectors[] = {
      {0x28, rte},         // A系列命令
      {0x2c, rte},         // F系列命令
      {0x80, TRAP0_WORK},  // trap #0
      {0x84, TRAP1_WORK},  // trap #1
      {0x88, TRAP2_WORK},  // trap #2
      {0x8c, TRAP3_WORK},  // trap #3
      {0x90, TRAP4_WORK},  // trap #4
      {0x94, TRAP5_WORK},  // trap #5
      {0x98, TRAP6_WORK},  // trap #6
      {0x9c, TRAP7_WORK},  // trap #7
      {0xa0, TRAP8_WORK},  // trap #8
      {0x118, 0},          // V-DISP
      {0x138, 0},          // CRTC-IRQ
  };

  WriteUWordSuper(rte, 0x4e73);
  WriteUWordSuper(rts, 0x4e75);

  // 割り込みルーチンの処理先設定
  const int len = sizeof(vectors) / sizeof(vectors[0]);
  for (int i = 0; i < len; i += 1) {
    ULong h = vectors[i].handler;

    // 最上位にベクタ番号を入れたアドレスがハイメモリ空間を指してしまうなら
    // ベクタ番号は取り除く。
    h = (ToPhysicalAddress(h) < BASE_ADDRESS_MAX) ? h : (h & ADDRESS_MASK);

    WriteULongSuper(vectors[i].vector, h);
  }

  // IOCSコールベクタの設定
  for (int i = 0; i < 256; i++) {
    WriteULongSuper(0x400 + i * 4, rts);
  }

  // IOCSワークの設定
  WriteUWordSuper(0x970, 79);  // 画面の桁数-1
  WriteUWordSuper(0x972, 24);  // 画面の行数-1

  // DOSコールベクタの設定
  for (int i = 0; i < 256; i++) {
    WriteULongSuper(0x1800 + i * 4, rts);
  }
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
    OP_info.rmem = 0;
    OP_info.rsize = 'N';
    OP_info.wmem = 0;
    OP_info.wsize = 'N';
    OP_info.mnemonic[0] = 0;
    if (superjsr_ret == pc) {
      SR_S_OFF();
      superjsr_ret = 0;
    }
    if (settings.trapPc != 0 && (ULong)pc == settings.trapPc) {
      printFmt("(run68) breakpoint:MPUがアドレス$%08xの命令を実行しました。\n",
               pc);
      settings.debug = true;
      if (stepcount != 0) {
        printFmt("(run68) breakpoint:%d counts left.\n", stepcount);
        stepcount = 0;
      }
    } else if (cwatchpoint != 0x4afc) {
      Span mem = GetReadableMemorySuper(pc, 2);
      if (mem.bufptr && cwatchpoint == PeekW(mem.bufptr)) {
        printFmt("(run68) watchpoint:MPUが命令$%04xを実行しました。\n",
                 cwatchpoint);
        settings.debug = true;
        if (stepcount != 0) {
          printFmt("(run68) breakpoint:%d counts left.\n", stepcount);
          stepcount = 0;
        }
      }
    }
    if (settings.debug) {
      settings.debug = false;
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
          settings.debug = true;
          goto NextInstruction;
        case RUN68_COMMAND_QUIT:
          cont_flag = false;
          goto NextInstruction;
      }
    } else if (stepcount != 0) {
      stepcount--;
      if (stepcount == 0) {
        settings.debug = true;
      }
    }
    if (pc & 1) {
      err68b("アドレスエラーが発生しました", pc, OPBuf_getentry(0)->pc);
      break;
    }
  NextInstruction:
    /* PCの値を保存する */
    OP_info.pc = pc;
    if (setjmp(jmp_when_abort) != 0) {
      settings.debug = true;
      continue;
    }
    if (prog_exec()) {
      running = false;
      if (debug_flag) {
        settings.debug = true;
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

static ULong init_env(ULong size, ULong parent) {
  Long buf = Malloc(MALLOC_FROM_LOWER, size, parent);
  if (buf < 0) return 0;

  WriteULongSuper(buf, size);
  WriteUByteSuper(buf + 4, 0);
  readenv_from_ini(ini_file_name, buf);
  return buf;
}

static void setHuman68kPathName(Human68kPathName *hpn, const char *path,
                                const char *name, const char *ext) {
  strncpy(hpn->path, path, sizeof(hpn->path));
  strncpy(hpn->name, name, sizeof(hpn->name));
  strncat(hpn->name, ext, sizeof(hpn->name) - strlen(hpn->name));
  hpn->nameLen = strlen(name);
  hpn->extLen = strlen(ext);
}

// メインメモリ、ハイメモリを確保し初期化する。
static bool initMachineMemory(Settings *settings, ULong *outHimemAdr) {
  if (!AllocateMachineMemory(settings, outHimemAdr)) {
    printFmt("メモリが確保できません。\n");
    return false;
  }

  WriteULongSuper(OSWORK_MEMORY_END, settings->mainMemorySize);
  return true;
}

// ハイメモリをメモリブロックのリンクリストに連結する
static void linkHimemToMemblkLink(ULong himemAdr, ULong himemSize,
                                  ULong parent) {
  if (himemSize == 0) return;

  // メモリ管理ポインタが0x00bffff0～、データ領域が0x00c00000～0x10000000-1
  // という構造の結合ブロックを作成する。
  Long buf = Malloc(MALLOC_FROM_HIGHER, 0, parent);
  if (buf < 0) return;
  WriteULongSuper(buf - SIZEOF_MEMBLK + MEMBLK_END, himemAdr);

  WriteULongSuper(OSWORK_MEMORY_END, himemAdr + himemSize);
}

static bool analyzeHimemOption(const char *arg) {
  static const unsigned long sizes[] = {0, 16, 32, 64, 128, 256, 384, 512, 768};
  const size_t sizes_len = sizeof(sizes) / sizeof(sizes[0]);

  const char *p = strchr(arg, '=');
  char *endptr;
  unsigned long mb = p ? strtoul(p + 1, &endptr, 10) : 0;
  if (*endptr) mb = 0;

  for (size_t i = 0; i < sizes_len; ++i) {
    if (sizes[i] == mb) {
      settings.highMemorySize = (ULong)(mb * 1024 * 1024);
      return true;
    }
  }

  print(
      "ハイメモリの容量は16,32,64,128,256,384,512,"
      "768のいずれかを指定する必要があります。\n");
  return false;
}

int main(int argc, char *argv[]) {
  char fname[89]; /* 実行ファイル名 */
  FILE *fp;       /* 実行ファイルのファイルポインタ */
  int i, j;
  bool restart;

  debug_flag = false;
  settings = defaultSettings;

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
            // print("MPU命令トレースフラグ=ON\n");
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
            int tpc = 0;
            sscanf(p, "%x", &tpc);
            settings.trapPc = (ULong)tpc;
            printFmt("MPU命令トラップフラグ=ON ADDR=$%08x\n", settings.trapPc);
          } else {
            invalid_flag = true;
          }
          break;
        case 'd':
          if (strcmp(argv[i], "-debug") != 0) {
            invalid_flag = true;
            break;
          }
          settings.debug = true;
          print("デバッガを起動します。\n");
          break;
        case 'f':
          settings.traceFunc = true;
          print("ファンクションコールトレースフラグ=ON\n");
          break;
        case 'h': {
          const char himem[] = "-himem=";
          if (strncmp(argv[i], himem, strlen(himem)) == 0) {
            if (!analyzeHimemOption(argv[i])) invalid_flag = true;
            break;
          }
          invalid_flag = true;
          break;
        }
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

  ULong himemAdr;
  if (!initMachineMemory(&settings, &himemAdr)) return EXIT_FAILURE;
  trap_table_make();

  // Human68kのPSPを作成
  const ULong humanPsp = HUMAN_HEAD;
  const ULong humanCodeSize = HUMAN_TAIL - (humanPsp + SIZEOF_PSP);
  BuildMemoryBlock(humanPsp, 0, 0, HUMAN_TAIL, 0);
  const ProgramSpec humanSpec = {humanCodeSize, 0};
  const Human68kPathName humanName = {"A:\\", "HUMAN.SYS", 0, 0};
  BuildPsp(humanPsp, -1, 0, 0x2000, humanPsp, &humanSpec, &humanName);
  WriteULongSuper(OSWORK_ROOT_PSP, humanPsp);
  nest_cnt = 0;

  WriteDrviceHeaders();

  // メインメモリの0番地からHuman68kの末尾までをスーパーバイザ領域に設定する。
  // エリアセットレジスタの仕様上は8KB単位。
  // Human68kの使用メモリを動的に変更する場合は、メモリブロックの末尾アドレスも
  // 同期させること。
  SetSupervisorArea(HUMAN_TAIL);

  linkHimemToMemblkLink(himemAdr, settings.highMemorySize, humanPsp);

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
  const Long programStack =
      Malloc(MALLOC_FROM_LOWER, DEFAULT_STACK_SIZE, humanPsp);
  const ULong stackBottom = programStack + DEFAULT_STACK_SIZE;

  // ここまではメモリブロックをメインメモリから確保している。
  // 以後はハイメモリからの確保も有効にする。
  SetAllocArea(ALLOC_AREA_UNLIMITED);

  // プログラム本体のロード用メモリを確保する。
  // ハイメモリ有効時でも16MBまでしか確保できないが、
  // 060turbo.sysの動作に合わせた仕様としている。
  const MallocResult child = MallocAll(humanPsp);

  if (humanEnv == 0 || cmdline == 0 || programStack < 0 || child.address < 0) {
    print("プロセス用のメモリを確保できません\n");
    return EXIT_FAILURE;
  }
  const ULong programPsp = child.address - SIZEOF_MEMBLK;

  /* プログラムをメモリに読み込む */
  Long prog_size = 0;  // プログラムサイズ(bss含む)
  Long prog_size2 = child.address + child.length;
  const Long entryAddress =
      prog_read(fp, fname, programPsp + SIZEOF_PSP, &prog_size, &prog_size2,
                print, EXEC_TYPE_DEFAULT);
  if (entryAddress < 0) {
    FreeMachineMemory();
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
  int ret = exec_notrap(&restart);

  /* 終了 */
  if (settings.traceFunc) {
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

  FreeMachineMemory();

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
