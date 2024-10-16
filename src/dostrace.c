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

#include "dostrace.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mem.h"

// _countof()
#define C(array) (sizeof(array) / sizeof(array[0]))

typedef struct {
  const UWord mode;
  const char name[14];
  const char* format;
} SubParams;

typedef struct {
  // モードごとの引数列とその配列サイズ。
  const SubParams* subParams;
  const size_t length;

  // trueなら先頭パラメータの上位バイトを無視する。
  // (下位バイトだけをモードとして扱う。DOS _EXEC用)
  const bool lowByte;
} SubCmd;

typedef struct {
  // ファンクションコール番号。
  const UWord code;

  // ファンクションコール名。NULLなら未知のコール。
  const char* name;

  // 引数列。NULLならエミュレート未実装、空文字列なら引数なし。
  const char* format;

  // モードによってスタック引数が違うコールの情報。
  const SubCmd* subCommand;
} DosCallParams;

static const SubParams kflushParams[] = {
    {0x0001, "gp", ""},            //
    {0x0006, "io", "code={c}"},    //
    {0x0007, "in", ""},            //
    {0x0008, "gc", ""},            //
    {0x000a, "gs", "inpptr={p}"},  //
};
static const SubCmd kflushCommand = {kflushParams, C(kflushParams), false};

static const SubParams conctrlParams[] = {
    {0x0000, "putc", "code={c}"},          //
    {0x0001, "print", "mesptr={s}"},       //
    {0x0002, "color", "atr={w}"},          //
    {0x0003, "locate", "x={w}, y={w}"},    //
    {0x0004, "down_s", ""},                //
    {0x0005, "up_s", ""},                  //
    {0x0006, "up", "n={w}"},               //
    {0x0007, "down", "n={w}"},             //
    {0x0008, "right", "n={w}"},            //
    {0x0009, "left", "n={w}"},             //
    {0x000a, "cls", "mod={w}"},            //
    {0x000b, "era", "mod={w}"},            //
    {0x000c, "ins", "n={w}"},              //
    {0x000d, "del", "n={w}"},              //
    {0x000e, "fnkmod", "mod={w}"},         //
    {0x000f, "window", "ys={w}, yl={w}"},  //
    {0x0010, "width", "mod={w}"},          //
    {0x0011, "curon", ""},                 //
    {0x0012, "curoff", ""},                //
};
static const SubCmd conctrlCommand = {conctrlParams, C(conctrlParams), false};

static const SubParams keyctrlParams[] = {
    {0x0000, "keyinp", ""},             //
    {0x0001, "keysns", ""},             //
    {0x0002, "sftsns", ""},             //
    {0x0003, "keybit", "group={w}"},    //
    {0x0004, "insmod", "insmode={w}"},  //
};
static const SubCmd keyctrlCommand = {keyctrlParams, C(keyctrlParams), false};

static const SubParams ioctrlParams[] = {
    {0x0000, "gt", "fileno={f}"},                    //
    {0x0001, "st", "fileno={f}, dt={w}"},            //
    {0x0002, "rh", "fileno={f}, ptr={p}, len={l}"},  //
    {0x0003, "wh", "fileno={f}, ptr={p}, len={l}"},  //
    {0x0004, "rd", "drive={d}, ptr={p}, len={l}"},   //
    {0x0005, "wd", "drive={d}, ptr={p}, len={l}"},   //
    {0x0006, "is", "fileno={f}"},                    //
    {0x0007, "os", "fileno={f}"},                    //
    // 0x0008は割り当てなし
    {0x0009, "dvgt", "drive={d}"},                         //
    {0x000a, "fdgt", "fileno={f}"},                        //
    {0x000b, "rtset", "count={w}, time={w}"},              //
    {0x000c, "dvctl", "fileno={f}, f_code={w}, ptr={p}"},  //
    {0x000d, "fdctl", "drive={d}, f_code={w}, ptr={p}"},   //
};
static const SubCmd ioctrlCommand = {ioctrlParams, C(ioctrlParams), false};

static const SubParams execParams[] = {
    {0x00, "loadexec", "file={s}, cmdline={s}, envptr={p}"},  //
    {0x01, "load", "file={s}, cmdline={s}, envptr={p}"},      //
    {0x02, "pathchk", "file={s}, cmdline={s}, envptr={p}"},   //
    {0x03, "loadonly", "file={s}, loadadr={p}, limit={p}"},   //
    {0x04, "execonly", "execadr={p}"},                        //
    {0x05, "bindno", "file={s}, file2={s}"},                  //
};
static const SubCmd execCommand = {execParams, C(execParams), true};

static const SubParams malloc2Params[] = {
    {0x0000, "low", "len={l}"},                      //
    {0x0001, "minimum", "len={l}"},                  //
    {0x0002, "high", "len={l}"},                     //
    {0x8000, "ex,low", "len={l}, oya_mcb={p}"},      //
    {0x8001, "ex,minimum", "len={l}, oya_mcb={p}"},  //
    {0x8002, "ex,high", "len={l}, oya_mcb={p}"},     //
};
static const SubCmd malloc2Command = {malloc2Params, C(malloc2Params), false};

static const SubParams assignParams[] = {
    {0x0000, "getassign", "buffer1={s}, buffer2={p}"},             //
    {0x0001, "makeassign", "buffer1={s}, buffer2={s}, mode={w}"},  //
    {0x0004, "rassign", "buffer1={s}"},                            //
};
static const SubCmd assignCommand = {assignParams, C(assignParams), false};

static const DosCallParams dosCalls[256] = {
    {0xff00, "EXIT", "", NULL},
    {0xff01, "GETCHAR", "", NULL},
    {0xff02, "PUTCHAR", "code={c}", NULL},
    {0xff03, "COMINP", NULL, NULL},
    {0xff04, "COMOUT", NULL, NULL},
    {0xff05, "PRNOUT", NULL, NULL},
    {0xff06, "INPOUT", "code={c}", NULL},
    {0xff07, "INKEY", "", NULL},
    {0xff08, "GETC", "", NULL},
    {0xff09, "PRINT", "mesptr={s}", NULL},
    {0xff0a, "GETS", "inpptr={p}", NULL},
    {0xff0b, "KEYSNS", "", NULL},
    {0xff0c, "KFLUSH", "mode={w}", &kflushCommand},
    {0xff0d, "FFLUSH", "", NULL},
    {0xff0e, "CHGDRV", "drive={D}", NULL},
    {0xff0f, "DRVCTRL", "mode={r}", NULL},

    {0xff10, "CONSNS", "", NULL},
    {0xff11, "PRNSNS", "", NULL},
    {0xff12, "CINSNS", "", NULL},
    {0xff13, "COUTSNS", "", NULL},
    {0xff14, NULL, NULL, NULL},
    {0xff15, NULL, NULL, NULL},
    {0xff16, NULL, NULL, NULL},
    {0xff17, "FATCHK", NULL, NULL},
    {0xff18, "HENDSP", NULL, NULL},
    {0xff19, "CURDRV", "", NULL},
    {0xff1a, "GETSS", "inpptr={p}", NULL},
    {0xff1b, "FGETC", "fileno={f}", NULL},
    {0xff1c, "FGETS", "buffer={p, NULL}, fileno={f}", NULL},
    {0xff1d, "FPUTC", "code={c, NULL}, fileno={f}", NULL},
    {0xff1e, "FPUTS", "mesptr={s, NULL}, fileno={f}", NULL},
    {0xff1f, "ALLCLOSE", "", NULL},

    {0xff20, "SUPER", "stack={p}", NULL},
    {0xff21, "FNCKEY", "mode={w, NULL}, buffer={p}", NULL},
    {0xff22, "KNJCTRL", NULL, NULL},
    {0xff23, "CONCTRL", "md={w}", &conctrlCommand},
    {0xff24, "KEYCTRL", "md={w}", &keyctrlCommand},
    {0xff25, "INTVCS", "intno={w, NULL}, jobadr={p}", NULL},
    {0xff26, "PSPSET", "pspadr={p}", NULL},
    {0xff27, "GETTIM2", "", NULL},
    {0xff28, "SETTIM2", "time={l}", NULL},
    {0xff29, "NAMESTS", "file={s, NULL}, buffer={p}", NULL},
    {0xff2a, "GETDATE", "", NULL},
    {0xff2b, "SETDATE", "date={w}", NULL},
    {0xff2c, "GETTIME", "", NULL},
    {0xff2d, "SETTIME", "time={w}", NULL},
    {0xff2e, "VERIFY", NULL, NULL},
    {0xff2f, "DUP0", NULL, NULL},

    {0xff30, "VERNUM", "", NULL},
    {0xff31, "KEEPPR", "prglen={l, NULL}, code={w}", NULL},
    {0xff32, "GETPDB", NULL, NULL},
    {0xff33, "BREAKCK", "flg={w}", NULL},
    {0xff34, "DRVXCHG", "old={d, NULL}, new={d}", NULL},
    {0xff35, "INTVCG", "intno={w}", NULL},
    {0xff36, "DSKFRE", "drive={d, NULL}, buffer={p}", NULL},
    {0xff37, "NAMECK", "file={s, NULL}, buffer={p}", NULL},
    {0xff38, NULL, NULL, NULL},
    {0xff39, "MKDIR", "nameptr={s}", NULL},
    {0xff3a, "RMDIR", "nameptr={s}", NULL},
    {0xff3b, "CHDIR", "nameptr={s}", NULL},
    {0xff3c, "CREATE", "nameptr={s, NULL}, atr={w}", NULL},
    {0xff3d, "OPEN", "nameptr={s, NULL}, mode={w}", NULL},
    {0xff3e, "CLOSE", "fileno={w}", NULL},
    {0xff3f, "READ", "fileno={f, NULL}, buffer={p, NULL}, len={l}", NULL},

    {0xff40, "WRITE", "fileno={f, NULL}, buffer={p, NULL}, len={l}", NULL},
    {0xff41, "DELETE", "nameptr={s}", NULL},
    {0xff42, "SEEK", "fileno={f, NULL}, offset={l, NULL}, mode={w}", NULL},
    {0xff43, "CHMOD", "nameptr={s, NULL}, atr={w}", NULL},
    {0xff44, "IOCTRL", "md={w}", &ioctrlCommand},
    {0xff45, "DUP", "fileno={f}", NULL},
    {0xff46, "DUP2", "fileno={f, NULL}, newno={f}", NULL},
    {0xff47, "CURDIR", "drive={d, NULL}, buffer={p}", NULL},
    {0xff48, "MALLOC", "len={l}", NULL},
    {0xff49, "MFREE", "memptr={p}", NULL},
    {0xff4a, "SETBLOCK", "memptr={p, NULL}, len={l}", NULL},
    {0xff4b, "EXEC", "module={b, NULL}, mode={b}", &execCommand},
    {0xff4c, "EXIT2", "code={w}", NULL},
    {0xff4d, "WAIT", "", NULL},
    {0xff4e, "FILES", "buffer={p, NULL}, file={s, NULL}, atr={w}", NULL},
    {0xff4f, "NFILES", "buffer={p}", NULL},

    {0xff50, "SETPDB", "pdbadr={p}", NULL},
    {0xff51, "GETPDB", "", NULL},
    {0xff52, "SETENV", NULL, NULL},
    {0xff53, "GETENV", "envname={s, NULL}, envptr={p, NULL}, buffer={p}", NULL},
    {0xff54, "VERIFYG", "", NULL},
    {0xff55, "COMMON", NULL, NULL},
    {0xff56, "RENAME", "old={s, NULL}, new={s}", NULL},
    {0xff57, "FILEDATE", "fileno={f, NULL}, datetime={l}", NULL},
    {0xff58, "MALLOC2", "md={w}", &malloc2Command},
    {0xff59, NULL, NULL, NULL},
    {0xff5a, "MAKETMP", "file={s, NULL}, atr={w}", NULL},
    {0xff5b, "NEWFILE", "file={s, NULL}, atr={w}", NULL},
    {0xff5c, "LOCK", NULL, NULL},
    {0xff5d, NULL, NULL, NULL},
    {0xff5e, NULL, NULL, NULL},
    {0xff5f, "ASSIGN", "md={w}", &assignCommand},

    {0xff60, "MALLOC3", NULL, NULL},
    {0xff61, "SETBLOCK2", NULL, NULL},
    {0xff62, "MALLOC4", NULL, NULL},
    {0xff63, "S_MALLOC2", NULL, NULL},
    {0xff64, NULL, NULL, NULL},
    {0xff65, NULL, NULL, NULL},
    {0xff66, NULL, NULL, NULL},
    {0xff67, NULL, NULL, NULL},
    {0xff68, NULL, NULL, NULL},
    {0xff69, NULL, NULL, NULL},
    {0xff6a, NULL, NULL, NULL},
    {0xff6b, NULL, NULL, NULL},
    {0xff6c, NULL, NULL, NULL},
    {0xff6d, NULL, NULL, NULL},
    {0xff6e, NULL, NULL, NULL},
    {0xff6f, NULL, NULL, NULL},

    {0xff70, NULL, NULL, NULL},
    {0xff71, NULL, NULL, NULL},
    {0xff72, NULL, NULL, NULL},
    {0xff73, NULL, NULL, NULL},
    {0xff74, NULL, NULL, NULL},
    {0xff75, NULL, NULL, NULL},
    {0xff76, NULL, NULL, NULL},
    {0xff77, NULL, NULL, NULL},
    {0xff78, NULL, NULL, NULL},
    {0xff79, NULL, NULL, NULL},
    {0xff7a, "FFLUSH_SET", NULL, NULL},
    {0xff7b, "OS_PATCH", NULL, NULL},
    {0xff7c, "GET_FCB_ADR", "fileno={f}", NULL},
    {0xff7d, "S_MALLOC", NULL, NULL},
    {0xff7e, "S_MFREE", NULL, NULL},
    {0xff7f, "S_PROCESS", NULL, NULL},

    {0xff80, "SETPDB", "pdbadr={p}", NULL},
    {0xff81, "GETPDB", "", NULL},
    {0xff82, "SETENV", NULL, NULL},
    {0xff83, "GETENV", "envname={s, NULL}, envptr={p, NULL}, buffer={p}", NULL},
    {0xff84, "VERIFYG", "", NULL},
    {0xff85, "COMMON", NULL, NULL},
    {0xff86, "RENAME", "old={s, NULL}, new={s}", NULL},
    {0xff87, "FILEDATE", "fileno={f, NULL}, datetime={l}", NULL},
    {0xff88, "MALLOC2", "md={w}", &malloc2Command},
    {0xff89, NULL, NULL, NULL},
    {0xff8a, "MAKETMP", "file={s, NULL}, atr={w}", NULL},
    {0xff8b, "NEWFILE", "file={s, NULL}, atr={w}", NULL},
    {0xff8c, "LOCK", NULL, NULL},
    {0xff8d, NULL, NULL, NULL},
    {0xff8e, NULL, NULL, NULL},
    {0xff8f, "ASSIGN", "md={w}", &assignCommand},

    {0xff90, "MALLOC3", NULL, NULL},
    {0xff91, "SETBLOCK2", NULL, NULL},
    {0xff92, "MALLOC4", NULL, NULL},
    {0xff93, "S_MALLOC2", NULL, NULL},
    {0xff94, NULL, NULL, NULL},
    {0xff95, NULL, NULL, NULL},
    {0xff96, NULL, NULL, NULL},
    {0xff97, NULL, NULL, NULL},
    {0xff98, NULL, NULL, NULL},
    {0xff99, NULL, NULL, NULL},
    {0xff9a, NULL, NULL, NULL},
    {0xff9b, NULL, NULL, NULL},
    {0xff9c, NULL, NULL, NULL},
    {0xff9d, NULL, NULL, NULL},
    {0xff9e, NULL, NULL, NULL},
    {0xff9f, NULL, NULL, NULL},

    {0xffa0, NULL, NULL, NULL},
    {0xffa1, NULL, NULL, NULL},
    {0xffa2, NULL, NULL, NULL},
    {0xffa3, NULL, NULL, NULL},
    {0xffa4, NULL, NULL, NULL},
    {0xffa5, NULL, NULL, NULL},
    {0xffa6, NULL, NULL, NULL},
    {0xffa7, NULL, NULL, NULL},
    {0xffa8, NULL, NULL, NULL},
    {0xffa9, NULL, NULL, NULL},
    {0xffaa, "FFLUSH_SET", NULL, NULL},
    {0xffab, "OS_PATCH", NULL, NULL},
    {0xffac, "GET_FCB_ADR", "fileno={f}", NULL},
    {0xffad, "S_MALLOC", NULL, NULL},
    {0xffae, "S_MFREE", NULL, NULL},
    {0xffaf, "S_PROCESS", NULL, NULL},

    {0xffb0, "TWON", NULL, NULL},
    {0xffb1, "MVDIR", NULL, NULL},
    {0xffb2, NULL, NULL, NULL},
    {0xffb3, NULL, NULL, NULL},
    {0xffb4, NULL, NULL, NULL},
    {0xffb5, NULL, NULL, NULL},
    {0xffb6, NULL, NULL, NULL},
    {0xffb7, NULL, NULL, NULL},
    {0xffb8, NULL, NULL, NULL},
    {0xffb9, NULL, NULL, NULL},
    {0xffba, NULL, NULL, NULL},
    {0xffbb, NULL, NULL, NULL},
    {0xffbc, NULL, NULL, NULL},
    {0xffbd, NULL, NULL, NULL},
    {0xffbe, NULL, NULL, NULL},
    {0xffbf, NULL, NULL, NULL},

    {0xffc0, NULL, NULL, NULL},
    {0xffc1, NULL, NULL, NULL},
    {0xffc2, NULL, NULL, NULL},
    {0xffc3, NULL, NULL, NULL},
    {0xffc4, NULL, NULL, NULL},
    {0xffc5, NULL, NULL, NULL},
    {0xffc6, NULL, NULL, NULL},
    {0xffc7, NULL, NULL, NULL},
    {0xffc8, NULL, NULL, NULL},
    {0xffc9, NULL, NULL, NULL},
    {0xffca, NULL, NULL, NULL},
    {0xffcb, NULL, NULL, NULL},
    {0xffcc, NULL, NULL, NULL},
    {0xffcd, NULL, NULL, NULL},
    {0xffce, NULL, NULL, NULL},
    {0xffcf, NULL, NULL, NULL},

    {0xffd0, NULL, NULL, NULL},
    {0xffd1, NULL, NULL, NULL},
    {0xffd2, NULL, NULL, NULL},
    {0xffd3, NULL, NULL, NULL},
    {0xffd4, NULL, NULL, NULL},
    {0xffd5, NULL, NULL, NULL},
    {0xffd6, NULL, NULL, NULL},
    {0xffd7, NULL, NULL, NULL},
    {0xffd8, NULL, NULL, NULL},
    {0xffd9, NULL, NULL, NULL},
    {0xffda, NULL, NULL, NULL},
    {0xffdb, NULL, NULL, NULL},
    {0xffdc, NULL, NULL, NULL},
    {0xffdd, NULL, NULL, NULL},
    {0xffde, NULL, NULL, NULL},
    {0xffdf, NULL, NULL, NULL},

    {0xffe0, "VMALLOC", NULL, NULL},
    {0xffe1, "VMFREE", NULL, NULL},
    {0xffe2, "VMALLOC2", NULL, NULL},
    {0xffe3, "VSETBLOCK", NULL, NULL},
    {0xffe4, "VEXEC", NULL, NULL},
    {0xffe5, NULL, NULL, NULL},
    {0xffe6, NULL, NULL, NULL},
    {0xffe7, NULL, NULL, NULL},
    {0xffe8, NULL, NULL, NULL},
    {0xffe9, NULL, NULL, NULL},
    {0xffea, NULL, NULL, NULL},
    {0xffeb, NULL, NULL, NULL},
    {0xffec, NULL, NULL, NULL},
    {0xffed, NULL, NULL, NULL},
    {0xffee, NULL, NULL, NULL},
    {0xffef, "GETFONT", NULL, NULL},

    {0xfff0, "EXITVC", NULL, NULL},
    {0xfff1, "CTRLVC", NULL, NULL},
    {0xfff2, "ERRJVC", NULL, NULL},
    {0xfff3, "DISKRED", NULL, NULL},
    {0xfff4, "DISKWRT", NULL, NULL},
    {0xfff5, "INDOSFLG", NULL, NULL},
    {0xfff6, "SUPER_JSR", "jobadr={p}", NULL},
    {0xfff7, "BUS_ERR", "s_adr={p, NULL}, d_adr={p, NULL}, md={w}", NULL},
    {0xfff8, "OPEN_PR", NULL, NULL},
    {0xfff9, "KILL_PR", NULL, NULL},
    {0xfffa, "GET_PR", NULL, NULL},
    {0xfffb, "SUSPEND_PR", NULL, NULL},
    {0xfffc, "SLEEP_PR", NULL, NULL},
    {0xfffd, "SEND_PR", NULL, NULL},
    {0xfffe, "TIME_PR", NULL, NULL},
    {0xffff, "CHANGE_PR", NULL, NULL},
};

static int codeToEscapeChar(int c) {
  switch (c) {
    default:
      return 0;

    case '\t':
      return 't';
    case '\r':
      return 'r';
    case '\n':
      return 'n';
  }
}

#define STRING_MAX_WIDTH 32

static void dumpString(const char* s) {
  char buf[STRING_MAX_WIDTH + 16] = {0};
  char* end = &buf[STRING_MAX_WIDTH];
  char* p = buf;

  while (*s) {
    if (end <= p) {
      strcpy(p, "...");
      p += strlen(p);
      break;
    }

    int c = *s++;
    if (isprint(c)) {
      *p++ = c;
    } else {
      int e = codeToEscapeChar(c);
      if (e) {
        *p++ = '\\';
        *p++ = e;
      } else {
        snprintf(p, 5, "\\x%02x", c);
        p += strlen(p);
      }
    }
  }
  *p = '\0';

  fputs(buf, stdout);
}

static const char* getDriveName(UWord d) {
  static const char drive[][2] = {
      "?", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
      "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
  };

  if (d == 0) return "curdrv";
  return drive[(d <= 26) ? d : 0];
}

// {b} ... UByte 16進数
static void printUByteHex(ULong* refParam) {
  printf("$%02x", (int)ReadParamUByte(refParam));
}

// {w} ... UWord 16進数
static void printUWordHex(ULong* refParam) {
  printf("$%04x", (int)ReadParamUWord(refParam));
}

// {c} ... UWord 16進数 + 下位バイトを文字として表示
static void printUWordChar(ULong* refParam) {
  UWord w = ReadParamUWord(refParam);

  int x = (int)w;
  int c = x & 0xff;

  if (isprint(c)) {
    printf("$%04x('%c')", x, c);
  } else {
    int e = codeToEscapeChar(c);
    if (e)
      printf("$%04x('\\%c')", x, c);
    else
      printf("$%04x", x);
  }
}

// {f} ... UWord 16進数 + ファイルハンドル
static void printUWordFileNo(ULong* refParam) {
  static const char stdfiles[][8] = {
      "stdin", "stdout", "stderr", "stdaux", "stdprn"  //
  };
  UWord w = ReadParamUWord(refParam);

  if (w < C(stdfiles))
    printf("$%04x(%s)", (int)w, stdfiles[w]);
  else
    printf("$%04x", (int)w);
}

// {d} ... UWord 16進数 + ドライブ番号(0=カレントドライブ 1=A:)
static void printUWordDrive(ULong* refParam) {
  UWord w = ReadParamUWord(refParam);
  printf("$%04x(%s:)", (int)w, getDriveName(w));
}

// {r} ... UWord 16進数 + モード&ドライブ番号(1=A:)
//   DOS _DRVCTRL用
static void printUWordDrvctrlMode(ULong* refParam) {
  UWord w = ReadParamUWord(refParam);
  printf("$%04x(md=%d,drive=%s:)", (int)w, w >> 8, getDriveName(w));
}

// {D} ... UWord 16進数 + ドライブ番号(0=A:)
//   DOS _CHGDRV用
static void printUWordDrive0(ULong* refParam) {
  UWord w = ReadParamUWord(refParam);
  printf("$%04x(%c:)", (int)w, (w <= 25) ? w + 'A' : '?');
}

// {l} ... ULong 16進数
// {p} ... ULong ポインタ
static void printULongHex(ULong* refParam) {
  printf("$%08x", (int)ReadParamULong(refParam));
}

// {s} ... ULong 文字列ポインタ + 文字列の内容
static void printULongString(ULong* refParam) {
  ULong ptr = ReadParamULong(refParam);
  const char* str = GetStringSuper(ptr);

  printf("$%08x(\"", (int)ptr);
  dumpString(str);
  fputs("\")", stdout);
}

static void printParam(const char* format, ULong* refParam) {
  switch (format[0]) {
    default:
      break;

    case 'b':
      printUByteHex(refParam);
      break;

    case 'w':
      printUWordHex(refParam);
      break;
    case 'c':
      printUWordChar(refParam);
      break;
    case 'f':
      printUWordFileNo(refParam);
      break;
    case 'r':
      printUWordDrvctrlMode(refParam);
      break;
    case 'd':
      printUWordDrive(refParam);
      break;
    case 'D':
      printUWordDrive0(refParam);
      break;

    case 'l':
    case 'p':
      printULongHex(refParam);
      break;
    case 's':
      printULongString(refParam);
      break;
  }
}

static const char* printFormat(const char* format, ULong* refParam) {
  while (format[0] != '\0') {
    const char* bracket = strchr(format, '{');
    if (bracket == NULL) break;

    // '{'の直前までを表示する
    fwrite(format, 1, (bracket - format), stdout);

    // 引数を一つ表示する
    format = bracket + 1;
    printParam(format, refParam);

    // '}'の直後まで飛ばす
    format = strchr(format, '}');
    if (!format) return "";
    format += 1;
  }

  return format;
}

static const char* printSubCommand(const SubCmd* subCommand, UWord mode,
                                   ULong* refParam) {
  if (subCommand->lowByte) mode &= 0x00ff;

  const size_t len = subCommand->length;
  for (size_t index = 0; index < len; index += 1) {
    const SubParams* subParams = &subCommand->subParams[index];
    if (subParams->mode == mode) {
      printf("(%s), ", subParams->name);
      return printFormat(subParams->format, refParam);
    }
  }

  return "(unknown sub command)";
}

void PrintDosCall(UByte code, ULong pc, ULong a6) {
  const DosCallParams* const dos = &dosCalls[code];

  // 命令のアドレスとオペコードを表示する
  printf("$%08x $ff%02x: ", pc, code);

  // ファンクションコール名を表示する
  if (!dos->name) {
    puts("(unknown dos call)");
    return;
  }
  const char* prefix = ((0x50 <= code) && (code <= 0x7f)) ? "V2_" : "";
  printf("DOS _%s%s ", prefix, dos->name);

  const char* format = dos->format;
  if (!format) {
    // エミュレート未実装ファンクションコール
    puts("(not implemented)");
    return;
  }

  UWord mode = dos->subCommand ? ReadUWordSuper(a6) : 0;

  // スタックに積まれた引数を表示する
  const char* footer = printFormat(format, &a6);
  if (dos->subCommand) {
    // モードによって異なる引数を表示する
    footer = printSubCommand(dos->subCommand, mode, &a6);
  }

  // 残りの文字列(または空文字列)と改行を表示する
  puts(footer);
}
