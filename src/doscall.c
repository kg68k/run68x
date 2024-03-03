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
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__GNUC__)
#include <conio.h>
#endif

#ifdef _WIN32
#include <direct.h>
#include <dos.h>
#include <io.h>
#include <windows.h>
#else
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#if defined(USE_ICONV)
#include <iconv.h>
#endif

#include "ansicolor-w32.h"
#include "dos_file.h"
#include "dos_memory.h"
#include "dostrace.h"
#include "host.h"
#include "human68k.h"
#include "mem.h"
#include "operate.h"
#include "run68.h"

static Long Gets(Long);
static Long Kflush(short);
static Long Ioctrl(short, Long);
static Long Dup(short);
static Long Dup2(short, short);
static Long Dskfre(short, Long);
static Long Open(char *, short);
static Long Close(short);
static Long Fgets(Long, short);
static Long Write(short, Long, Long);
#if defined(__APPLE__) || defined(__linux__) || defined(__EMSCRIPTEN__)
static Long Write_conv(short, void *, size_t);
#endif
static Long Delete(char *);
static Long Seek(short, Long, short);
static Long Rename(Long, Long);
static Long Chmod(Long, short);
static Long Files(Long, Long, short);
static Long Nfiles(Long);
static Long Getdate(void);
static Long Setdate(short);
static Long Gettime(int);
static Long Settim2(Long);
static Long Namests(Long, Long);
static Long Nameck(Long, Long);
static Long Conctrl(short, Long);
static Long Keyctrl(short, Long);
static void Fnckey(short, Long);
static Long Intvcg(UWord);
static Long Intvcs(UWord, Long);
static Long Assign(short, Long);
static Long Getfcb(short);
static Long Exec01(ULong, Long, Long, int);
static Long Exec2(Long, Long, Long);
static Long Exec3(ULong, Long, Long);
static void Exec4(Long);

#ifndef _WIN32
void CloseHandle(FILE *fp) { fclose(fp); }

int _dos_getfileattr(char *name, void *ret) {
  printf("_dos_getfileattr(\"%s\")\n", name);

  return 1;
}

int _dos_setfileattr(char *name, short attr) {
  printf("_dos_setfileattr(\"%s\", %d)\n", name, attr);

  return 1;
}

int _dos_write(int fd, void *data, int size, size_t *res) {
  //	printf("_dos_write()\n" );
  *res = write(fd, data, size);
  return 0;
}

void _flushall() {
  //	printf("_flushall()\n" );
}

char _getch() {
  printf("_getch()\n");
  return getchar();
}

char _getche() {
  printf("_getche()\n");
  return getchar();
}

void dos_getdrive(Long *drv) {
  //	printf("dos_getdrive(%p)\n", drv );
  *drv = 1;  // 1 = A:
}

void dos_setdrive(Long drv, Long *dmy) {
  //	printf("dos_setdrive(%d, %p)\n", drv, dmy );
}

int _kbhit() {
  //	printf("_kbhit()\n");
  return 1;
}

int kbhit() {
  //	printf("kbhit()\n");
  return 1;
}

char ungetch(char c) {
  printf("ungetch()\n");
  return 0;
}
#endif

// DOS _MKDIR (0xff39)
static Long Mkdir(Long name) {  //
  return HOST_DOS_MKDIR(name);
}

// DOS _RMDIR (0xff3a)
static Long Rmdir(Long name) {  //
  return HOST_DOS_RMDIR(name);
}

// DOS _CHDIR (0xff3b)
static Long Chdir(Long name) {  //
  return HOST_DOS_CHDIR(name);
}

// DOS _READ (0xff3b)
static Long DosRead(ULong param) {
  UWord fileno = ReadParamUWord(&param);
  ULong buffer = ReadParamULong(&param);
  ULong length = ReadParamULong(&param);

  return Read(fileno, buffer, length);
}

// DOS _CREATE (0xff3c)
static Long DosCreate(ULong param) {
  ULong file = ReadParamULong(&param);
  UWord atr = ReadParamUWord(&param);

  return CreateNewFile(file, atr, false);
}

// DOS _CURDIR (0xff47)
static Long Curdir(short drv, char *buf_ptr) {
  return HOST_DOS_CURDIR(drv, buf_ptr);
}

// DOS _MALLOC (0xff48)
static Long DosMalloc(ULong param) {
  return Malloc(MALLOC_FROM_LOWER, ReadParamULong(&param), psp[nest_cnt]);
}

// DOS _MFREE (0xff49)
static Long DosMfree(ULong param) { return Mfree(ReadParamULong(&param)); }

// DOS _SETBLOCK (0xff4a)
static Long DosSetblock(ULong param) {
  ULong adr = ReadParamULong(&param);
  ULong size = ReadParamULong(&param);
  return Setblock(adr, size);
}

// DOS _GETENV (0xff53, 0xff53)
static Long DosGetenv(ULong param) {
  ULong name = ReadParamULong(&param);
  ULong env = ReadParamULong(&param);
  ULong buf = ReadParamULong(&param);

  const char *s = Getenv(GetStringSuper(name), env);
  if (!s) return DOSE_ILGFNC;

  WriteStringSuper(buf, s);
  return DOSE_SUCCESS;
}

// 環境変数領域から環境変数を検索する。
const char *Getenv(const char *name, ULong env) {
  if (env == 0) {
    env = ReadULongSuper(psp[nest_cnt] + PSP_ENV_PTR);
  }
  if (env == (ULong)-1) return NULL;

  // 環境変数領域の先頭4バイトは領域サイズで、その次から文字列が並ぶ。
  ULong kv = env + 4;
  const size_t len = strlen(name);

  for (;;) {
    char *p = GetStringSuper(kv);
    if (*p == '\0') break;

    if (memcmp(p, name, len) == 0 && p[len] == '=') {
      return p + len + strlen("=");
    }
    kv += strlen(p) + 1;
  }
  return NULL;
}

// DOS _FILEDATE (0xff57, 0xff87)
static Long DosFiledate(ULong param) {
  UWord fileno = ReadParamUWord(&param);
  ULong dt = ReadParamULong(&param);
  return HOST_DOS_FILEDATE(fileno, dt);
}

// DOS _MALLOC2 (0xff58, 0xff88)
static Long DosMalloc2(ULong param) {
  UWord mode = ReadParamUWord(&param);
  UByte modeByte = mode & 0xff;
  if (modeByte > MALLOC_FROM_HIGHER) return DOSE_ILGPARM;
  ULong size = ReadParamULong(&param);
  ULong parent =
      (mode & 0x8000) ? ReadParamULong(&param) : (ULong)psp[nest_cnt];

  return Malloc(modeByte, size, parent);
}

// DOS _MAKETMP (0xff5a, 0xff8a)
static Long DosMaketmp(ULong param) {
  ULong path = ReadParamULong(&param);
  UWord atr = ReadParamUWord(&param);
  return Maketmp(path, atr);
}

// DOS _NEWFILE (0xff5b, 0xff8b)
static Long DosNewfile(ULong param) {
  ULong file = ReadParamULong(&param);
  UWord atr = ReadParamUWord(&param);

  return CreateNewFile(file, atr, true);
}

// DOS _MALLOC3 (0xff60, 0xff90)
static Long DosMalloc3(ULong param) {
  if (settings.highMemorySize == 0) return DOSE_ILGFNC;

  return MallocHuge(MALLOC_FROM_LOWER, ReadParamULong(&param), psp[nest_cnt]);
}

// DOS _SETBLOCK2 (0xff61, 0xff91)
static Long DosSetblock2(ULong param) {
  if (settings.highMemorySize == 0) return DOSE_ILGFNC;

  ULong adr = ReadParamULong(&param);
  ULong size = ReadParamULong(&param);
  return SetblockHuge(adr, size);
}

// DOS _MALLOC4 (0xff62, 0xff91)
static Long DosMalloc4(ULong param) {
  if (settings.highMemorySize == 0) return DOSE_ILGFNC;

  UWord mode = ReadParamUWord(&param);
  UByte modeByte = mode & 0xff;
  if (modeByte > MALLOC_FROM_HIGHER) return DOSE_ILGPARM;
  ULong size = ReadParamULong(&param);
  ULong parent =
      (mode & 0x8000) ? ReadParamULong(&param) : (ULong)psp[nest_cnt];

  return MallocHuge(modeByte, size, parent);
}

// DOS _BUS_ERR (0xfff7)
static Long DosBusErr(ULong param) {
  ULong s_adr = ReadParamULong(&param);
  ULong d_adr = ReadParamULong(&param);
  UWord size = ReadParamUWord(&param);

  if (size == 1) {
    // バイトアクセス
  } else if (size == 2 || size == 4) {
    // ワード、ロングワードアクセス
    if ((s_adr & 1) != 0 || (d_adr & 1) != 0)
      return DOSE_ILGFNC;  // 奇数アドレス
  } else {
    return DOSE_ILGFNC;  // サイズが不正
  }

  Span r = GetReadableMemorySuper(s_adr, size);
  if (!r.bufptr) return 2;  // 読み込み時にバスエラー発生
  Span w = GetWritableMemorySuper(d_adr, size);
  if (!w.bufptr) return 1;  // 書き込み時にバスエラー発生

  if (size == 1)
    PokeB(w.bufptr, PeekB(r.bufptr));
  else if (size == 2)
    PokeW(w.bufptr, PeekW(r.bufptr));
  else
    PokeL(w.bufptr, PeekL(r.bufptr));

  return 0;
}

// ファイルを閉じてFILEINFOを未使用状態に戻す
static bool CloseFile(FILEINFO *finfop) {
  finfop->is_opened = false;
  return HOST_CLOSE_FILE(finfop);
}

// 開いている(オープン中でない)ファイル番号を探す
static Long find_free_file(void) {
  int i;

  for (i = HUMAN68K_USER_FILENO_MIN; i < FILE_MAX; i++) {
    if (!finfo[i].is_opened) {
      return (Long)i;
    }
  }
  return (Long)-1;
}

// 現在のプロセスが開いたファイルを全て閉じる
void close_all_files(void) {
  int i;

  for (i = HUMAN68K_USER_FILENO_MIN; i < FILE_MAX; i++) {
    if (finfo[i].nest == nest_cnt) {
      CloseFile(&finfo[i]);
    }
  }
}

// DOS _EXIT、DOS _EXIT2
static bool Exit2(Long exit_code) {
  Mfree(0);
  close_all_files();
  rd[0] = exit_code;
  if (nest_cnt == 0) {
    return true;
  }
  sr = mem_get(psp[nest_cnt] + PSP_PARENT_SR, S_WORD);
  Mfree(psp[nest_cnt] + SIZEOF_MEMBLK);
  nest_cnt--;
  pc = nest_pc[nest_cnt];
  ra[7] = nest_sp[nest_cnt];
  return false;
}

/*
 　機能：DOSCALLを実行する
 戻り値： true = 実行終了
         false = 実行継続
 */
bool dos_call(UByte code) {
  char *data_ptr = 0;
  Long data;
  Long buf;
  Long len;
  short srt;
  short fhdl;
  Long c = 0;
#ifdef _WIN32
  DWORD st;
#endif
  Long stack_adr = ra[7];

  if (settings.traceFunc) {
    PrintDosCall(code, pc - 2, stack_adr);
  }

#ifdef TRACE
  printf("trace: DOSCALL  0xFF%02X PC=%06lX\n", code, pc);
#endif
  if (code >= 0x80 && code <= 0xAF) code -= 0x30;

  switch (code) {
    case 0x01: /* GETCHAR */
#ifdef _WIN32
      FlushFileBuffers(finfo[1].host.handle);
#endif
      rd[0] = (_getche() & 0xFF);
      break;
    case 0x02: /* PUTCHAR */
    {
      // WriteW32()が文字列長を無視して常に文字列末尾まで走査するので、
      // 出力する1文字の直後に必ずNUL文字を置くこと。
      char c[2] = {(char)ReadUWordSuper(stack_adr), '\0'};
#ifdef _WIN32
      FILEINFO *finfop = &finfo[1];
      if (GetConsoleMode(finfop->host.handle, &st) != 0) {
        // 非リダイレクト
        WriteW32(1, finfop->host.handle, c, 1);
      } else {
        Long nwritten;
        /* Win32API */
        WriteFile(finfop->host.handle, c, 1, (LPDWORD)&nwritten, NULL);
      }
#else
      Write_conv(1, c, 1);
#endif
      rd[0] = 0;
      break;
    }
    case 0x06: /* INPOUT */
      srt = (short)mem_get(stack_adr, S_WORD);
      srt &= 0xFF;
      if (srt >= 0xFE) {
#ifdef _WIN32
        FILEINFO *finfop = &finfo[0];
        INPUT_RECORD ir[3];
        DWORD read_len = 0;
        rd[0] = 0;
        PeekConsoleInputA(finfop->host.handle, ir, 1, (LPDWORD)&read_len);
        if (read_len != 0) {
          int keydown =
              ir[0].EventType == KEY_EVENT && ir[0].Event.KeyEvent.bKeyDown;
          if (srt == 0xff || !keydown) {
            ReadConsoleInputA(finfop->host.handle, ir, _countof(ir),
                              (LPDWORD)&read_len);
          }
          // 制限: この方法だと2バイト文字には対応できないので無視している
          if (read_len == 1 && ir[0].EventType == KEY_EVENT &&
              ir[0].Event.KeyEvent.bKeyDown) {
            rd[0] = (UByte)ir[0].Event.KeyEvent.uChar.AsciiChar;
          }
        }
#else
        rd[0] = 0;
        if (kbhit() != 0) {
          c = _getch();
          if (c == 0x00) {
            c = _getch();
          }
          if (srt == 0xFE) ungetch(c);
          rd[0] = c;
        }
#endif
      } else {
        putchar(srt);
        rd[0] = 0;
      }
      break;
    case 0x07: /* INKEY */
    case 0x08: /* GETC */
#ifdef _WIN32
      FlushFileBuffers(finfo[1].host.handle);
#endif
      c = _getch();
      if (c == 0x00) {
        c = _getch();
        c = 0x1B;
      }
      rd[0] = c;
      break;
    case 0x09: /* PRINT */
      data_ptr = GetStringSuper(mem_get(stack_adr, S_LONG));
      len = strlen(data_ptr);
#ifdef _WIN32
      {
        FILEINFO *finfop = &finfo[1];
        if (GetConsoleMode(finfop->host.handle, &st) != 0) {
          WriteW32(1, finfop->host.handle, data_ptr, len);
        } else {
          Long nwritten;
          /* Win32API */
          WriteFile(finfop->host.handle, data_ptr, len, (LPDWORD)&nwritten,
                    NULL);
        }
      }
#else
      Write_conv(1, data_ptr, (unsigned)len);
#endif
      rd[0] = 0;
      break;
    case 0x0A: /* GETS */
      buf = mem_get(stack_adr, S_LONG);
      rd[0] = Gets(buf);
      break;
    case 0x0B: /* KEYSNS */
      if (_kbhit() != 0)
        rd[0] = -1;
      else
        rd[0] = 0;
      break;
    case 0x0C: /* KFLUSH */
      srt = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Kflush(srt);
      break;
    case 0x0D: /* FFLUSH */
#ifdef _WIN32
      /* オープン中の全てのファイルをフラッシュする。*/
      for (int i = 5; i < FILE_MAX; i++) {
        if (finfo[i].is_opened) FlushFileBuffers(finfo[i].host.handle);
      }
#else
      _flushall();
#endif
      rd[0] = 0;
      break;
    case 0x0E: /* CHGDRV */
      srt = (short)mem_get(stack_adr, S_WORD);
#ifdef _WIN32
      {
        char drv[3];
        sprintf(drv, "%c:", srt + 'A');
        if (SetCurrentDirectory(drv)) {
          /* When succeeded. */
          rd[0] = srt;
        }
      }
#else
      srt += 1;
      int drv = 0;
      dos_setdrive(srt, &drv);
      rd[0] = drv;

#endif
      // Verify
#ifdef _WIN32
      {
        char drv[512];
        BOOL b = GetCurrentDirectoryA(sizeof(drv), drv);
        if (b && strlen(drv) != 0 && (drv[0] - 'A') == rd[0]) {
          /* OK, nothing to do. */
        } else {
          rd[0] = -15; /* ドライブ指定誤り */
        }
      }
#else
      dos_getdrive(&drv);
      srt += 1;
      if (srt != drv) rd[0] = -15;
#endif
      break;
    case 0x0F: /* DRVCTRL(何もしない) */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (srt > 26 && srt < 256)
        rd[0] = -15; /* ドライブ指定誤り */
      else
        rd[0] = 0x02; /* READY */
      break;
    case 0x10: /* CONSNS */
      _flushall();
      rd[0] = -1;
      break;
    case 0x11: /* PRNSNS */
    case 0x12: /* CINSNS */
    case 0x13: /* COUTSNS */
      _flushall();
      rd[0] = 0;
      break;
    case 0x19: /* CURDRV */
#ifdef _WIN32
    {
      char path[512];
      BOOL b = GetCurrentDirectory(sizeof(path), path);
      if (b && strlen(path) != 0) {
        rd[0] = path[0] - 'A';
      } else {
        rd[0] = -15; /* ドライブ指定誤り */
      }
    }
#else
      dos_getdrive(&drv);
      rd[0] = drv - 1;
#endif
    break;
    case 0x1B: /* FGETC */
      fhdl = (short)mem_get(stack_adr, S_WORD);
      if (finfo[fhdl].mode == 1) {
        rd[0] = -1;
      } else {
#ifdef _WIN32
        DWORD read_len = 0;
        INPUT_RECORD ir;
        FILEINFO *finfop = &finfo[fhdl];
        if (GetFileType(finfop->host.handle) == FILE_TYPE_CHAR) {
          /* 標準入力のハンドルがキャラクタタイプだったら、ReadConsoleを試してみる。*/
          while (true) {
            BOOL b = ReadConsoleInput(finfop->host.handle, &ir, 1,
                                      (LPDWORD)&read_len);
            if (b == FALSE) {
              /* コンソールではなかった。*/
              ReadFile(finfop->host.handle, &c, 1, (LPDWORD)&read_len, NULL);
              break;
            }
            if (read_len == 1 && ir.EventType == KEY_EVENT &&
                ir.Event.KeyEvent.bKeyDown) {
              c = ir.Event.KeyEvent.uChar.AsciiChar;
              if (0x01 <= c && c <= 0xff) break;
            }
          }
        } else {
          ReadFile(finfop->host.handle, &c, 1, (LPDWORD)&read_len, NULL);
        }
        if (read_len == 0) c = EOF;
        rd[0] = c;
#else
        rd[0] = fgetc(finfo[fhdl].host.fp);
#endif
      }
      break;
    case 0x1C: /* FGETS */
      data = mem_get(stack_adr, S_LONG);
      fhdl = (short)mem_get(stack_adr + 4, S_WORD);
      rd[0] = Fgets(data, fhdl);
      break;
    case 0x1D: /* FPUTC */
    {
      char c[2] = {(char)ReadUWordSuper(stack_adr), '\0'};
      fhdl = (short)mem_get(stack_adr + 2, S_WORD);
#ifdef _WIN32
      FILEINFO *finfop = &finfo[fhdl];
      if (GetConsoleMode(finfop->host.handle, &st) != 0 &&
          (fhdl == 1 || fhdl == 2)) {
        // 非リダイレクトで標準出力か標準エラー出力
        WriteW32(fhdl, finfop->host.handle, c, 1);
        rd[0] = 0;
      } else {
        int fail =
            WriteFile(finfop->host.handle, c, 1, (LPDWORD)&len, NULL) == FALSE;
        rd[0] = fail ? 0 : 1;
      }
#else
      rd[0] = (Write_conv(fhdl, c, 1) == EOF) ? 0 : 1;
#endif
      break;
    }
    case 0x1E: /* FPUTS */
      data = mem_get(stack_adr, S_LONG);
      fhdl = (short)mem_get(stack_adr + 4, S_WORD);
      data_ptr = GetStringSuper(data);
#ifdef _WIN32
      if ((fhdl == 1 || fhdl == 2) &&
          GetConsoleMode(finfo[1].host.handle, &st) != FALSE) {
        // 非リダイレクトで標準出力か標準エラー出力
        len =
            WriteW32(fhdl, finfo[fhdl].host.handle, data_ptr, strlen(data_ptr));
      } else {
        WriteFile(finfo[fhdl].host.handle, data_ptr, strlen(data_ptr),
                  (LPDWORD)&len, NULL);
      }
      rd[0] = len;
#else
      if (Write_conv(fhdl, data_ptr, strlen(data_ptr)) == -1) {
        rd[0] = 0;
      } else {
        rd[0] = strlen(data_ptr);
      }
#endif
      break;
    case 0x1F: /* ALLCLOSE */
      close_all_files();
      rd[0] = 0;
      break;
    case 0x20: /* SUPER */
      data = mem_get(stack_adr, S_LONG);
      if (data == 0) {
        /* user -> super */
        if (SR_S_REF() != 0) {
          rd[0] = -26;
        } else {
          rd[0] = ra[7];
          usp = ra[7];
          SR_S_ON();
        }
      } else {
        /* super -> user */
        ra[7] = data;
        rd[0] = 0;
        usp = 0;
        SR_S_OFF();
      }
      break;
    case 0x21: /* FNCKEY */
      srt = (short)mem_get(stack_adr, S_WORD);
      buf = mem_get(stack_adr + 2, S_LONG);
      Fnckey(srt, buf);
      rd[0] = 0;
      break;
    case 0x23: /* CONCTRL */
      srt = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Conctrl(srt, stack_adr + 2);
      break;
    case 0x24: /* KEYCTRL */
      srt = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Keyctrl(srt, stack_adr + 2);
      break;
    case 0x25: /* INTVCS */
      srt = (short)mem_get(stack_adr, S_WORD);
      data = mem_get(stack_adr + 2, S_LONG);
      rd[0] = Intvcs(srt, data);
      break;
    case 0x27: /* GETTIM2 */
      rd[0] = Gettime(1);
      break;
    case 0x28: /* SETTIM2 */
      data = mem_get(stack_adr, S_LONG);
      rd[0] = Settim2(data);
      break;
    case 0x29: /* NAMESTS */
      data = mem_get(stack_adr, S_LONG);
      buf = mem_get(stack_adr + 4, S_LONG);
      rd[0] = Namests(data, buf);
      break;
    case 0x2A: /* GETDATE */
      rd[0] = Getdate();
      break;
    case 0x2B: /* SETDATE */
      srt = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Setdate(srt);
      break;
    case 0x2C: /* GETTIME */
      rd[0] = Gettime(0);
      break;
    case 0x30: /* VERNUM */
      rd[0] = 0x36380302;
      break;
    case 0x32: /* GETDPB */
      srt = (short)mem_get(stack_adr, S_WORD);
      rd[0] = -1;
      break;
    case 0x33: /* BREAKCK */
      rd[0] = 1;
      break;
    case 0x34:     /* DRVXCHG */
      rd[0] = -15; /* ドライブ指定誤り */
      break;
    case 0x35: /* INTVCG */
      srt = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Intvcg(srt);
      break;
    case 0x36: /* DSKFRE */
      srt = (short)mem_get(stack_adr, S_WORD);
      buf = mem_get(stack_adr + 2, S_LONG);
      rd[0] = Dskfre(srt, buf);
      break;
    case 0x37: /* NAMECK */
      data = mem_get(stack_adr, S_LONG);
      buf = mem_get(stack_adr + 4, S_LONG);
      rd[0] = Nameck(data, buf);
      break;
    case 0x39: /* MKDIR */
      data = mem_get(stack_adr, S_LONG);
      rd[0] = Mkdir(data);
      break;
    case 0x3A: /* RMDIR */
      data = mem_get(stack_adr, S_LONG);
      rd[0] = Rmdir(data);
      break;
    case 0x3B: /* CHDIR */
      data = mem_get(stack_adr, S_LONG);
      rd[0] = Chdir(data);
      break;
    case 0x3c:  // CREATE
      rd[0] = DosCreate(stack_adr);
      break;
    case 0x3D: /* OPEN */
      data = mem_get(stack_adr, S_LONG);
      srt = (short)mem_get(stack_adr + 4, S_WORD);
      rd[0] = Open(GetStringSuper(data), srt);
      break;
    case 0x3E: /* CLOSE */
      srt = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Close(srt);
      break;
    case 0x3F: /* READ */
      rd[0] = DosRead(stack_adr);
      break;
    case 0x40: /* WRITE */
      srt = (short)mem_get(stack_adr, S_WORD);
      data = mem_get(stack_adr + 2, S_LONG);
      len = mem_get(stack_adr + 6, S_LONG);
      rd[0] = Write(srt, data, len);
      break;
    case 0x41: /* DELETE */
      data = mem_get(stack_adr, S_LONG);
      rd[0] = Delete(GetStringSuper(data));
      break;
    case 0x42: /* SEEK */
      fhdl = (short)mem_get(stack_adr, S_WORD);
      data = mem_get(stack_adr + 2, S_LONG);
      srt = (short)mem_get(stack_adr + 6, S_WORD);
      rd[0] = Seek(fhdl, data, srt);
      break;
    case 0x43: /* CHMOD */
      data = mem_get(stack_adr, S_LONG);
      srt = (short)mem_get(stack_adr + 4, S_WORD);
      rd[0] = Chmod(data, srt);
      break;
    case 0x44: /* IOCTRL */
      srt = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Ioctrl(srt, stack_adr + 2);
      break;
    case 0x45: /* DUP */
      fhdl = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Dup(fhdl);
      break;
    case 0x46: /* DUP2 */
      srt = (short)mem_get(stack_adr, S_WORD);
      fhdl = (short)mem_get(stack_adr + 2, S_WORD);
      rd[0] = Dup2(srt, fhdl);
      break;
    case 0x47: /* CURDIR */
      srt = (short)mem_get(stack_adr, S_WORD);
      data = mem_get(stack_adr + 2, S_LONG);
      rd[0] = Curdir(srt, GetStringSuper(data));
      break;
    case 0x48: /* MALLOC */
      rd[0] = DosMalloc(stack_adr);
      break;
    case 0x49: /* MFREE */
      rd[0] = DosMfree(stack_adr);
      break;
    case 0x4A: /* SETBLOCK */
      rd[0] = DosSetblock(stack_adr);
      break;
    case 0x4B: /* EXEC */
      srt = (short)mem_get(stack_adr, S_WORD);
      data = mem_get(stack_adr + 2, S_LONG);
      buf = len = 0;
      if (srt < 4) {
        buf = mem_get(stack_adr + 6, S_LONG);
        len = mem_get(stack_adr + 10, S_LONG);
      }
      switch (srt) {
        case 0:
          rd[0] = Exec01(data, buf, len, 0);
          break;
        case 1:
          rd[0] = Exec01(data, buf, len, 1);
          break;
        case 2:
          rd[0] = Exec2(data, buf, len);
          break;
        case 3:
          rd[0] = Exec3(data, buf, len);
          break;
        case 4:
          Exec4(data);
          break;
        default:
          err68("DOSCALL EXEC(5)が実行されました");
          return true;
      }
      break;
    case 0x4E: /* FILES */
      buf = mem_get(stack_adr, S_LONG);
      data = mem_get(stack_adr + 4, S_LONG);
      srt = (short)mem_get(stack_adr + 8, S_WORD);
      rd[0] = Files(buf, data, srt);
      break;
    case 0x4F: /* NFILES */
      buf = mem_get(stack_adr, S_LONG);
      rd[0] = Nfiles(buf);
      break;
    case 0x51: /* GETPDB */
      rd[0] = psp[nest_cnt] + SIZEOF_MEMBLK;
      break;
    case 0x53:  // GETENV
      rd[0] = DosGetenv(stack_adr);
      break;
    case 0x54: /* VERIFYG */
      rd[0] = 1;
      break;
    case 0x56: /* RENAME */
      data = mem_get(stack_adr, S_LONG);
      buf = mem_get(stack_adr + 4, S_LONG);
      rd[0] = Rename(data, buf);
      break;
    case 0x57:  // FILEDATE
      rd[0] = DosFiledate(stack_adr);
      break;
    case 0x58:  // MALLOC2
      rd[0] = DosMalloc2(stack_adr);
      break;
    case 0x5a:  // MAKETMP
      rd[0] = DosMaketmp(stack_adr);
      break;
    case 0x5b:  // NEWFILE
      rd[0] = DosNewfile(stack_adr);
      break;
    case 0x5F: /* ASSIGN */
      srt = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Assign(srt, stack_adr + 2);
      break;
    case 0x60:
      rd[0] = DosMalloc3(stack_adr);
      break;
    case 0x61:
      rd[0] = DosSetblock2(stack_adr);
      break;
    case 0x62:
      rd[0] = DosMalloc4(stack_adr);
      break;
    case 0x7C: /* GETFCB */
      fhdl = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Getfcb(fhdl);
      break;
    case 0xF6: /* SUPER_JSR */
      data = mem_get(stack_adr, S_LONG);
      ra[7] -= 4;
      mem_set(ra[7], pc, S_LONG);
      if (SR_S_REF() == 0) {
        superjsr_ret = pc;
        SR_S_ON();
      }
      pc = data;
      break;
    case 0xf7:  // BUS_ERR
      rd[0] = DosBusErr(stack_adr);
      break;

    case 0x4C: /* EXIT2 */
      if (Exit2(mem_get(stack_adr, S_WORD))) return true;
      break;
    case 0x00: /* EXIT */
      if (Exit2(0)) return true;
      break;

    case 0x31: /* KEEPPR */
    {
      len = mem_get(stack_adr, S_LONG);
      UWord exit_code = mem_get(stack_adr + 4, S_WORD);
      Mfree(0);
      close_all_files();
      if (nest_cnt == 0) return true;

      Setblock(psp[nest_cnt] + SIZEOF_MEMBLK, len + SIZEOF_PSP - SIZEOF_MEMBLK);
      mem_set(psp[nest_cnt] + MEMBLK_PARENT, 0xFF, S_BYTE);
      sr = (short)mem_get(psp[nest_cnt] + PSP_PARENT_SR, S_WORD);
      nest_cnt--;
      pc = nest_pc[nest_cnt];
      ra[7] = nest_sp[nest_cnt];
      rd[0] = exit_code;
    } break;

    default:
      rd[0] = DOSE_ILGFNC;
      break;
  }
  return false;
}

/*
 　機能：
     DOSCALL GETSを実行する
   パラメータ：
     Long  buf    <in>    入力バッファアドレス
   戻り値：
     Long  入力文字数
 */
static Long Gets(Long buf) {
  char str[256];

  UByte max = ReadUByteSuper(buf);
  Long len = gets2(str, max);
  WriteUByteSuper(buf + 1, len);
  WriteStringSuper(buf + 2, str);
  return len;
}

/*
 　機能：
     DOSCALL KFLUSHを実行する
   パラメータ：
     Long  buf    <in>    モード
   戻り値：
     Long  キーコード等
 */
static Long Kflush(short mode) {
  UByte c;

  switch (mode) {
    case 0x01:
      return (_getche() & 0xFF);
    case 0x07:
    case 0x08:
      c = _getch();
      if (c == 0x00) {
        c = _getch();
        c = 0x1B;
      }
      return (c);
    case 0x0A:
      return (0);
    default:
      return (0);
  }
}

/*
 　機能：
     DOSCALL IOCTRLを実行する
   パラメータ：
     short    mode      <in>    モード
     Long  stack_adr <in>    スタックアドレス
   戻り値：
     Long  バイト数等
 */
static Long Ioctrl(short mode, Long stack_adr) {
  short fno;

  switch (mode) {
    case 0:
      fno = (short)mem_get(stack_adr, S_WORD);
      if (fno == 0) return (0x80C1);
      if (fno == 1 || fno == 2) return (0x80C2);
      return (0);
    case 6:
      fno = (short)mem_get(stack_adr, S_WORD);
      if (fno == 0) return (0xFF); /* 入力可 */
      if (fno < 5) return (0);
      if (!finfo[fno].is_opened) return 0;
      if (finfo[fno].mode == 0 || finfo[fno].mode == 2) return (0xFF);
      return (0);
    case 7:
      fno = (short)mem_get(stack_adr, S_WORD);
      if (fno == 1 || fno == 2) return (0xFF); /* 出力可 */
      if (fno < 5) return (0);
      if (!finfo[fno].is_opened) return 0;
      if (finfo[fno].mode == 1 || finfo[fno].mode == 2) return (0xFF);
      return (0);
    default:
      return (0);
  }
}

/*
 　機能：
     DOSCALL DUPを実行する
   パラメータ：
     short    org       <in>    オリジナルファイルハンドル?
   戻り値：
     Long  複写先のハンドルまたはエラーコード
 */
static Long Dup(short org) {
  if (org < 5) return (-14);

  Long ret = find_free_file();
  if (ret < 0) return -4;  // オープンしているファイルが多すぎる

  finfo[ret] = finfo[org];
  return ret;
}

/*
 　機能：DOSCALL DUP2を実行する
 戻り値：エラーコード
 */
static Long Dup2(short org, short new) {
  if (new < 5 || org < 5) return (-14);

  if (new >= FILE_MAX) return (-14); /* 無効なパラメータ */

  if (finfo[new].is_opened) {
    if (Close(new) < 0) return -14;
  }

  finfo[new] = finfo[org];
  return 0;
}

/*
 　機能：
     DOSCALL DSKFREを実行する
   パラメータ：
     Long  drv       <in>    ドライブ番号(0)
     Long  buf       <in>    メモリアドレス
   戻り値：
     Long  ディスクの空き容量(バイト>0)
              エラーコード(<0)
 */
static Long Dskfre(short drv, Long buf) {
#ifdef _WIN32
  ULong SectorsPerCluster, BytesPerSector, NumberOfFreeClusters,
      TotalNumberOfClusters;

  BOOL b = GetDiskFreeSpaceA(
      NULL, (LPDWORD)&SectorsPerCluster, (LPDWORD)&BytesPerSector,
      (LPDWORD)&NumberOfFreeClusters, (LPDWORD)&TotalNumberOfClusters);
  if (b == FALSE) return (-15);
  NumberOfFreeClusters &= 0xFFFF;
  mem_set(buf, NumberOfFreeClusters, S_WORD);
  TotalNumberOfClusters &= 0xFFFF;
  mem_set(buf + 2, TotalNumberOfClusters, S_WORD);
  SectorsPerCluster &= 0xFFFF;
  mem_set(buf + 4, SectorsPerCluster, S_WORD);
  BytesPerSector &= 0xFFFF;
  mem_set(buf + 6, BytesPerSector, S_WORD);
  return NumberOfFreeClusters * SectorsPerCluster * BytesPerSector;
#else
  return -15;
#endif
}

#ifndef _WIN32
static char *to_slash(size_t size, char *buf, const char *path) {
  if (strlen(path) >= size) return NULL;

  char *p = strncpy(buf, path, size);
  char *root = (p[0] && p[1] == ':') ? p + strlen("A:") : p;
  do {
    if (*p == '\\') *p = '/';
  } while (*p++);

  return root;
}
#endif

// ファイル名後ろの空白をつめる。
static void trimRight(char *s) {
  for (char *t = s + strlen(s) - 1; s <= t && *t == ' '; t -= 1) *t = '\0';
}

// 新しいファイルを作成する。
Long CreateNewFile(ULong file, UWord atr, bool newfile) {
  const char *filename = GetStringSuper(file);

  // パス名の加工用にバッファを確保してコピーする。
  size_t bufSize = strlen(filename) + 1;
  char *buf = malloc(bufSize);
  if (!buf) return DOSE_ILGFNAME;
#ifdef _WIN32
  char *p = strcpy(buf, filename);
#else
  char *p = to_slash(bufSize, buf, filename);
  if (!p) {
    free(buf);
    return DOSE_ILGFNAME;
  }
#endif
  trimRight(p);

  Human68kPathName hpn;
  if (!HOST_CANONICAL_PATHNAME(p, &hpn)) {
    free(buf);
    return DOSE_ILGFNAME;
  }
  free(buf);

  // ホスト上のファイルをオープンするためのパス名を作る。
  char fullpath[HUMAN68K_PATH_MAX + 1];
#ifdef _WIN32
  p = strcat(strcpy(fullpath, hpn.path), hpn.name);
#else
  {
    char fullpath2[HUMAN68K_PATH_MAX + 1];
    strcat(strcpy(fullpath, hpn.path), hpn.name);
    p = to_slash(sizeof(fullpath2), fullpath2, fullpath);
    if (!p) return DOSE_ILGFNAME;
    if (!HOST_CONVERT_FROM_SJIS(p, fullpath, sizeof(fullpath)))
      return DOSE_ILGFNAME;
    p = fullpath;
  }
#endif

  Long ret = find_free_file();
  if (ret < 0) return DOSE_MFILE;  // オープンしているファイルが多すぎる

#ifdef _WIN32
  DWORD dwCreationDisposition = newfile ? CREATE_NEW : CREATE_ALWAYS;
  HANDLE handle =
      CreateFile(p, GENERIC_WRITE | GENERIC_READ, 0, NULL,
                 dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    DWORD e = GetLastError();
    return (e == ERROR_FILE_EXISTS) ? DOSE_EXISTFILE : DOSE_DISKFULL;
  }
  finfo[ret].host.handle = handle;
#else
  if (newfile) {
    FILE *fp = fopen(p, "rb");
    if (fp != NULL) {
      fclose(fp);
      return DOSE_EXISTFILE;  // 既に存在している
    }
  }
  FILE *fp = fopen(p, "w+b");
  if (fp == NULL) return DOSE_DISKFULL;
  finfo[ret].host.fp = fp;
#endif

  finfo[ret].is_opened = true;
  finfo[ret].mode = 2;
  finfo[ret].nest = nest_cnt;
  return (ret);
}

/*
 　機能：DOSCALL OPENを実行する
 戻り値：ファイルハンドル(負ならエラーコード)
 */
static Long Open(char *p, short mode) {
#ifdef _WIN32
  DWORD md;
#else
  const char *mdstr;
#endif
  int len;
  Long i;

#ifndef _WIN32
  char buf[89];
  p = to_slash(sizeof(buf), buf, p);
  if (p == NULL) return DOSE_ILGFNAME;
    // printf("Open(\"%s\", 0x%02x)\n", p, mode);
#endif

  switch (mode) {
    case 0: /* 読み込みオープン */
#ifdef _WIN32
      md = GENERIC_READ;
#else
      mdstr = "rb";
#endif
      break;
    case 1: /* 書き込みオープン */
    {
#ifdef _WIN32
      HANDLE handle = CreateFile(p, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, NULL);

      if (handle == INVALID_HANDLE_VALUE) return -2;
      CloseHandle(handle);
      md = GENERIC_WRITE;
#else
      FILE *fp = fopen(p, "rb");
      if (fp == NULL) return -2;  // ファイルは見つからない
      fclose(fp);
      mdstr = "r+b";
#endif
      break;
    }
    case 2: /* 読み書きオープン */
#ifdef _WIN32
      md = GENERIC_READ | GENERIC_WRITE;
#else
      mdstr = "r+b";
#endif
      break;
    default:
      return (-12); /* アクセスモードが異常 */
  }

  /* ファイル名後ろの空白をつめる */
  len = strlen(p);
  for (i = len - 1; i >= 0 && p[i] == ' '; i--) p[i] = '\0';

  if ((len = strlen(p)) > 88) return (-13); /* ファイル名の指定誤り */

  Long ret = find_free_file();
  if (ret < 0) return -4;  // オープンしているファイルが多すぎる
#ifdef _WIN32
  HANDLE handle =
      CreateFile(p, md, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle == INVALID_HANDLE_VALUE) return (mode == 1) ? -23 : -2;
  finfo[ret].host.handle = handle;
#else
  FILE *fp = fopen(p, mdstr);
  if (fp == NULL) {
    if (mode == 1)
      return -23;  // ディスクがいっぱい
    else
      return -2;  // ファイルは見つからない
  }
  finfo[ret].host.fp = fp;
#endif

  finfo[ret].is_opened = true;
  finfo[ret].mode = mode;
  finfo[ret].nest = nest_cnt;
  return (ret);
}

/*
 　機能：DOSCALL CLOSEを実行する
 戻り値：エラーコード
 */
static Long Close(short hdl) {
  if (hdl <= HUMAN68K_SYSTEM_FILENO_MAX) return DOSE_SUCCESS;
  if (!finfo[hdl].is_opened) return DOSE_BADF;  // オープンされていない
  if (!CloseFile(&finfo[hdl]))
    return DOSE_ILGPARM;  // 無効なパラメータでコールした

  return 0;
}

/*
 　機能：DOSCALL FGETSを実行する
 戻り値：エラーコード
 */
static Long Fgets(Long adr, short hdl) {
  char buf[257];
  size_t len;

  if (!finfo[hdl].is_opened) return -6;  // オープンされていない
  if (finfo[hdl].mode == 1) return (-1);

  UByte max = ReadUByteSuper(adr);
#ifdef _WIN32
  {
    int i = 0;
    while (i < max) {
      DWORD read_len;
      char c;

      if (ReadFile(finfo[hdl].host.handle, &c, 1, (LPDWORD)&read_len, NULL) ==
          FALSE)
        return -1;
      if (read_len == 0) {
        if (i > 0) break;
        return -1;
      }
      if (c == '\r') continue;
      if (c == '\n') break;

      buf[i++] = c;
    }
    buf[i] = '\0';
    len = i;
  }
#else
  {
    if (fgets(buf, max, finfo[hdl].host.fp) == NULL) return -1;
    char *s = buf;
    char *d = buf;
    char c;
    while ((c = *s++) != '\0') {
      if (c != '\r' && c != '\n') *d++ = c;
    }
    *d = '\0';
    len = strlen(buf);
  }
#endif

  WriteUByteSuper(adr + 1, len);
  WriteStringSuper(adr + 2, buf);

  return len;
}

/*
 　機能：DOSCALL WRITEを実行する
 戻り値：書き込んだバイト数(負ならエラーコード)
 */
static Long Write(short hdl, Long buf, Long len) {
  Long write_len = 0;

  if (!finfo[hdl].is_opened) return -6;  // オープンされていない
  if (len == 0) return 0;

  Span mem = GetReadableMemorySuper(buf, len);
  if (!mem.bufptr) {
    // 指定範囲の先頭ないし途中でバスエラー発生
    throwBusErrorOnRead(buf + mem.length);
  }

#ifdef _WIN32
  unsigned len2;
  WriteFile(finfo[hdl].host.handle, mem.bufptr, mem.length, &len2, NULL);
  write_len = len2;
  if (finfo[hdl].host.handle == GetStdHandle(STD_OUTPUT_HANDLE))
    FlushFileBuffers(finfo[hdl].host.handle);
#else
  write_len = Write_conv(hdl, mem.bufptr, mem.length);
#endif

  return write_len;
}

#if defined(__APPLE__) || defined(__linux__) || defined(__EMSCRIPTEN__)
static Long Write_conv(short hdl, void *buf, size_t size) {
  Long write_len;
  FILE *fp = finfo[hdl].host.fp;

  if (fp == NULL) return (-6);

  if (!isatty(fileno(fp))) {
    write_len = fwrite(buf, 1, size, fp);
  } else {
#if defined(USE_ICONV)
    static char prev_char = 0;
    iconv_t icd = iconv_open("UTF-8", "Shift_JIS");

    write_len = 0;

    while (size > 0) {
      char sjis_buf[2048];
      char utf8_buf[4096];
      size_t sjis_buf_size;
      char *sjis_buf_p;
      size_t sjis_bytes;
      char *utf8_buf_p = utf8_buf;
      size_t utf8_bytes = sizeof(utf8_buf);
      size_t utf8_bytes_prev = utf8_bytes;
      int res;

      if (prev_char) {
        sjis_buf[0] = prev_char;
        sjis_buf_p = sjis_buf + 1;
        sjis_buf_size = sizeof(sjis_buf) - 1;
        prev_char = 0;
      } else {
        sjis_buf_p = sjis_buf;
        sjis_buf_size = sizeof(sjis_buf);
      }

      sjis_bytes = size > sjis_buf_size ? sjis_buf_size : size;
      memcpy(sjis_buf_p, buf, sjis_bytes);
      buf += sjis_bytes;
      size -= sjis_bytes;
      write_len += sjis_bytes;
      sjis_bytes += sjis_buf_p - sjis_buf;
      sjis_buf_p = sjis_buf;

      res = iconv(icd, &sjis_buf_p, &sjis_bytes, &utf8_buf_p, &utf8_bytes);
      if (res < 0 && errno == EINVAL) {
        prev_char = *sjis_buf_p;
      }
      fwrite(utf8_buf, 1, utf8_bytes_prev - utf8_bytes, fp);
    }
    iconv_close(icd);

    fflush(fp);
#else
    write_len = fwrite(buf, 1, size, fp);
#endif
  }
  return write_len;
}
#endif

/*
 　機能：DOSCALL DELETEを実行する
 戻り値：ファイルハンドル(負ならエラーコード)
 */
static Long Delete(char *p) {
  if (remove(p) != 0) return (errno == ENOENT) ? DOSE_NOENT : DOSE_ILGFNAME;
  return DOSE_SUCCESS;
}

/*
 　機能：DOSCALL SEEKを実行する
 戻り値：先頭からのオフセット(負ならエラーコード)
 */
static Long Seek(short hdl, Long offset, short mode) {
  int sk;

  if (!finfo[hdl].is_opened) return -6;  // オープンされていない

#ifdef _WIN32
  switch (mode) {
    case 0:
      sk = FILE_BEGIN;
      break;
    case 1:
      sk = FILE_CURRENT;
      break;
    case 2:
      sk = FILE_END;
      break;
    default:
      return (-14); /* 無効なパラメータ */
  }
  DWORD ret = SetFilePointer(finfo[hdl].host.handle, offset, NULL, sk);
  if (ret < 0) return -25;  // 指定の位置にシークできない
  return (Long)ret;
#else
  switch (mode) {
    case 0:
      sk = SEEK_SET;
      break;
    case 1:
      sk = SEEK_CUR;
      break;
    case 2:
      sk = SEEK_END;
      break;
    default:
      return (-14); /* 無効なパラメータ */
  }
  if (fseek(finfo[hdl].host.fp, offset, sk) != 0)
    return (-25); /* 指定の位置にシークできない */
  return ftell(finfo[hdl].host.fp);
#endif
}

/*
 　機能：DOSCALL RENAMEを実行する
 戻り値：エラーコード
 */
static Long Rename(Long old, Long new1) {
  char *old_ptr = GetStringSuper(old);
  char *new_ptr = GetStringSuper(new1);

  errno = 0;
  if (rename(old_ptr, new_ptr) != 0) {
    if (errno == EACCES)
      return (-22); /* ファイルがあってリネームできない */
    else
      return (-7); /* ファイル名指定誤り */
  }

  return (0);
}

/*
 　機能：DOSCALL CHMODを実行する
 戻り値：エラーコード
 */
static Long Chmod(Long adr, short atr) {
  char *name_ptr = GetStringSuper(adr);

  if (atr == -1) {
    /* 読み出し */
    ULong ret;
#ifdef _WIN32
    if ((ret = GetFileAttributesA(name_ptr)) == 0xFFFFFFFF) return -2;
#else
    if (_dos_getfileattr(name_ptr, &ret) != 0) return (-2); /* ファイルがない */
#endif
    return (ret);
  } else {
    atr &= 0x3F;
    errno = 0;
#ifdef _WIN32
    if (SetFileAttributesA(name_ptr, atr) == FALSE) {
#else
    if (_dos_setfileattr(name_ptr, atr) != 0) {
#endif
      if (errno == ENOENT)
        return (-2); /* ファイルがない */
      else
        return (-19); /* 書き込み禁止 */
    }
    return (atr);
  }
}

/*
   機能：
     DOSCALL FILESを実行する
   パラメータ：
     Long  buf       <in>    ファイル検索バッファのアドレス
     Long  name      <in>    ファイル名(ワイルドカード含む)へのポインタ
     short    atr       <in>    属性
   戻り値：
     エラーコード
 */
static Long Files(Long buf, Long name, short atr) {
  char *name_ptr = GetStringSuper(name);

  ULong bufSize = 53;
  bool exMode = (buf & 0x80000000) ? true : false;
  if (exMode) {
    // buf &= ~0x80000000;
    // bufSize = 141;
    return DOSE_ILGFNC;
  }

  Span mem = GetWritableMemorySuper(buf, bufSize);
  if (!mem.bufptr) throwBusErrorOnWrite(buf + mem.length);
  char *buf_ptr = mem.bufptr;

#ifdef _WIN32
  WIN32_FIND_DATA f_data;
  HANDLE handle;

  /* 最初にマッチするファイルを探す。*/
  /* FindFirstFileEx()はWindowsNTにしかないのでボツ
     handle = FindFirstFileEx(name_ptr, FindExInfoStandard,
              (LPVOID)&f_data, FindExSearchNameMatch, NULL, 0);
  */

  /* 最初のファイルを検索する。*/
  handle = FindFirstFile(name_ptr, &f_data);
  /* 予約領域をセット */
  buf_ptr[0] = atr;                  /* ファイルの属性 */
  buf_ptr[1] = 0;                    /* ドライブ番号(not used) */
  *((HANDLE *)&buf_ptr[2]) = handle; /* サーチハンドル */
  {
    bool b = handle != INVALID_HANDLE_VALUE;
    /* 属性の一致するファイルが見つかるまで繰返し検索する。*/
    while (b) {
      unsigned char fatr;
      fatr = f_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? 0x01 : 0;
      fatr |= f_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ? 0x02 : 0;
      fatr |= f_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM ? 0x04 : 0;
      /*			fatr |= f_data.dwFileAttributes &
       * FILE_ATTRIBUTE_VOLUMEID ? 0x08 : 0; */
      fatr |= f_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? 0x10 : 0;
      fatr |= f_data.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ? 0x20 : 0;
      if (fatr & buf_ptr[0] || (fatr == 0 && (buf_ptr[0] & 0x20))) {
        /* ATRをセット */
        buf_ptr[21] = fatr;
        break; /* 指定された属性のファイルが見つかった。*/
      }
      b = FindNextFile(handle, &f_data) != FALSE;
    }
    if (!b) return (-2);
  }
  /* DATEとTIMEをセット */
  {
    SYSTEMTIME st;
    unsigned short s;
    FileTimeToSystemTime(&f_data.ftLastWriteTime, &st);
    s = (st.wHour << 11) + (st.wMinute << 5) + st.wSecond / 2;
    buf_ptr[22] = (s & 0xff00) >> 8;
    buf_ptr[23] = s & 0xff;
    s = ((st.wYear - 1980) << 9) + (st.wMonth << 5) + st.wDay;
    buf_ptr[24] = (s & 0xff00) >> 8;
    buf_ptr[25] = s & 0xff;
  }
  /* FILELENをセット */
  buf_ptr[26] = (unsigned char)((f_data.nFileSizeLow & 0xff000000) >> 24);
  buf_ptr[27] = (unsigned char)((f_data.nFileSizeLow & 0x00ff0000) >> 16);
  buf_ptr[28] = (unsigned char)((f_data.nFileSizeLow & 0x0000ff00) >> 8);
  buf_ptr[29] = (unsigned char)(f_data.nFileSizeLow & 0x000000ff);
  /* PACKEDNAMEをセット */
  strncpy(&buf_ptr[30], f_data.cFileName, 22);
  buf_ptr[30 + 22] = 0;

  return 0;

#else
  char slbuf[89];
  name_ptr = to_slash(sizeof(slbuf), slbuf, name_ptr);
  if (name_ptr == NULL) return DOSE_ILGFNAME;

  {
    FILE *fp = fopen(name_ptr, "rb");
    if (fp != NULL) {
      fclose(fp);

      /* 予約領域をセット */
      buf_ptr[0] = atr; /* ファイルの属性 */
      buf_ptr[1] = 0;   /* ドライブ番号(not used) */
                        //			*((HANDLE*)&buf_ptr[2]) = handle; /*
                        // サーチハンドル */
      /* DATEとTIMEをセット */
      {
        /*
                                        SYSTEMTIME st;
                                        unsigned short s;
                                        FileTimeToSystemTime(&f_data.ftLastWriteTime,
           &st); s = (st.wHour << 11) + (st.wMinute << 5) + st.wSecond / 2;
                                        buf_ptr[22] = (s & 0xff00) >> 8;
                                        buf_ptr[23] = s & 0xff;
                                        s =((st.wYear - 1980) << 9) +
                                        (st.wMonth << 5) +
                                        st.wDay;
                                        buf_ptr[24] = (s & 0xff00) >> 8;
                                        buf_ptr[25] = s & 0xff;
        */
      }
      // FILELENをセット
      size_t size = 64;
      buf_ptr[26] = (unsigned char)((size & 0xff000000) >> 24);
      buf_ptr[27] = (unsigned char)((size & 0x00ff0000) >> 16);
      buf_ptr[28] = (unsigned char)((size & 0x0000ff00) >> 8);
      buf_ptr[29] = (unsigned char)(size & 0x000000ff);
      /* PACKEDNAMEをセット */
      strncpy(&buf_ptr[30], name_ptr, 22);
      buf_ptr[30 + 22] = 0;

      return 0;
    }
  }

  char *path = name_ptr;
  DIR *dir;
  struct dirent *dent;

  dir = opendir(path);
  printf("opendir(%s)=%p\n", path, dir);
  if (dir == NULL) {
    //		perror(path);
  } else {
    while ((dent = readdir(dir)) != NULL) {
      printf("%s\n", dent->d_name);
    }
    closedir(dir);
  }
  printf("DOSCALL FILES:not defined yet %s %d\n", __FILE__, __LINE__);
  return -1;
#endif
}

/*
 　機能：DOSCALL NFILESを実行する
 戻り値：エラーコード
 */
static Long Nfiles(Long buf) {
  Span mem = GetWritableMemorySuper(buf, SIZEOF_FILES);
  if (!mem.bufptr) throwBusErrorOnWrite(buf + mem.length);

#ifdef _WIN32
  WIN32_FIND_DATA f_data;
  char *buf_ptr = mem.bufptr;
  short atr = buf_ptr[0]; /* 検索すべきファイルの属性 */

  {
    /* todo:buf_ptrの指す領域から必要な情報を取り出して、f_dataにコピーする。*/
    /* 2秒→100nsに変換する。*/
    SYSTEMTIME st;
    unsigned short s;

    s = *((unsigned short *)&buf_ptr[24]);
    st.wYear = ((s & 0xfe00) >> 9) + 1980;
    st.wMonth = (s & 0x01e0) >> 5;
    st.wDay = (s & 0x1f);
    s = *((unsigned short *)&buf_ptr[22]);
    st.wHour = (s & 0xf800) >> 11;
    st.wMinute = (s & 0x07e0) >> 5;
    st.wSecond = (s & 0x001f);
    st.wMilliseconds = 0;
    SystemTimeToFileTime(&st, &f_data.ftLastWriteTime);

    f_data.nFileSizeHigh = 0;
    f_data.nFileSizeLow = *((ULong *)&buf_ptr[29]);
    /* ファイルのハンドルをバッファから取得する。*/
    HANDLE handle = *((HANDLE *)&buf_ptr[2]);
    bool b = FindNextFile(handle, &f_data) != FALSE;
    /* 属性の一致するファイルが見つかるまで繰返し検索する。*/
    while (b) {
      unsigned char fatr;
      fatr = f_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? 0x01 : 0;
      fatr |= f_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ? 0x02 : 0;
      fatr |= f_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM ? 0x04 : 0;
      /*			fatr |= f_data.dwFileAttributes &
       * FILE_ATTRIBUTE_VOLUMEID ? 0x08 : 0; */
      fatr |= f_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? 0x10 : 0;
      fatr |= f_data.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ? 0x20 : 0;
      if (fatr & buf_ptr[0] || (fatr == 0 && (buf_ptr[0] & 0x20))) {
        /* ATRをセット */
        buf_ptr[21] = fatr;
        break; /* 指定された属性のファイルが見つかった。*/
      }
      b = FindNextFile(handle, &f_data) != FALSE;
    }
    if (!b) {
      return -2;
    }
  }

  /* DATEとTIMEをセット */
  {
    SYSTEMTIME st;
    unsigned short s;
    FileTimeToSystemTime(&f_data.ftLastWriteTime, &st);
    s = (st.wHour << 11) + (st.wMinute << 5) + st.wSecond / 2;
    buf_ptr[22] = (s & 0xff00) >> 8;
    buf_ptr[23] = s & 0xff;
    s = ((st.wYear - 1980) << 9) + (st.wMonth << 5) + st.wDay;
    buf_ptr[24] = (s & 0xff00) >> 8;
    buf_ptr[25] = s & 0xff;
  }
  /* FILELENをセット */
  buf_ptr[26] = (unsigned char)((f_data.nFileSizeLow & 0xff000000) >> 24);
  buf_ptr[27] = (unsigned char)((f_data.nFileSizeLow & 0x00ff0000) >> 16);
  buf_ptr[28] = (unsigned char)((f_data.nFileSizeLow & 0x0000ff00) >> 8);
  buf_ptr[29] = (unsigned char)(f_data.nFileSizeLow & 0x000000ff);
  /* PACKEDNAMEをセット */
  strncpy(&buf_ptr[30], f_data.cFileName, 22);
  buf_ptr[30 + 22] = 0;

  return 0;

#else
  printf("DOSCALL NFILES:not defined yet %s %d\n", __FILE__, __LINE__);
  return -1;
#endif
}

/*
 　機能：DOSCALL GETDATEを実行する
 戻り値：現在の日付
 */
static Long Getdate() {
  Long ret;

#ifdef _WIN32
  SYSTEMTIME stime;
  // GetSystemTime(&stime);
  GetLocalTime(&stime);
  ret = ((Long)(stime.wDayOfWeek) << 16) + (((Long)(stime.wYear) - 1980) << 9) +
        ((Long)(stime.wMonth) << 5) + (Long)(stime.wDay);
#else
  struct tm tm;
  time_t t = time(NULL);
  localtime_r(&t, &tm);
  //	printf("%04d/%02d/%02d %d %02d:%02d:%02d\n",
  //		   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
  //		   tm.tm_wday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  ret = (tm.tm_wday << 16) + ((tm.tm_year + (1980 - 1900)) << 9) +
        (tm.tm_mon << 5) + tm.tm_mday;

#endif
  return (ret);
}

/*
 　機能：DOSCALL SETDATEを実行する
 戻り値：エラーコード
 */
static Long Setdate(short dt) {
#ifdef _WIN32
  SYSTEMTIME stime;
  BOOL b;
  stime.wYear = (dt >> 9) & 0x7F + 1980;
  stime.wMonth = (dt >> 5) & 0xF;
  stime.wDay = dt & 0x1f;
  stime.wSecond = 0;
  stime.wMilliseconds = 0;
  // b = SetSystemTime(&stime);
  b = SetLocalTime(&stime);
  if (!b) return -14; /* パラメータ不正 */
#else
  printf("DOSCALL SETDATE:not defined yet %s %d\n", __FILE__, __LINE__);
#endif
  return (0);
}

/*
 　機能：DOSCALL GETTIME / GETTIME2を実行する
 戻り値：現在の時刻
 */
static Long Gettime(int flag) {
  Long ret;
#ifdef _WIN32
  SYSTEMTIME stime;
  // GetSystemTime(&stime);
  GetLocalTime(&stime);
  if (flag == 0)
    // ret = stime.wHour << 11 + stime.wMinute << 5 + stime.wSecond >> 1;
    ret = ((Long)(stime.wHour) << 11) + ((Long)(stime.wMinute) << 5) +
          ((Long)(stime.wSecond) >> 1);
  else
    // ret = stime.wHour << 16 + stime.wMinute << 8 + stime.wSecond;
    ret = ((Long)(stime.wHour) << 16) + ((Long)(stime.wMinute) << 8) +
          (Long)(stime.wSecond);
#else

  struct tm tm;
  time_t t = time(NULL);
  localtime_r(&t, &tm);
  //	printf("%04d/%02d/%02d %d %02d:%02d:%02d\n",
  //		   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
  //		   tm.tm_wday, tm.tm_hour, tm.tm_min, tm.tm_sec);

  if (flag == 0)
    ret = (tm.tm_hour << 11) + (tm.tm_min << 5) + (tm.tm_sec >> 1);
  else
    ret = (tm.tm_hour << 16) + (tm.tm_min << 8) + tm.tm_sec;

#endif
  return (ret);
}

/*
 　機能：DOSCALL SETTIM2を実行する
 戻り値：エラーコード
 */
static Long Settim2(Long tim) {
#ifdef _WIN32
  SYSTEMTIME stime;
  BOOL b;
  stime.wYear = (tim >> 16) & 0x1F;
  stime.wMonth = (tim >> 8) & 0x3F;
  stime.wDay = tim & 0x3f;
  stime.wSecond = 0;
  stime.wMilliseconds = 0;
  b = SetSystemTime(&stime);
  if (!b) return -14; /* パラメータ不正 */
#else
  printf("DOSCALL SETTIM2:not defined yet %s %d\n", __FILE__, __LINE__);
#endif
  return (0);
}

/*
   機能：
     DOSCALL NAMESTSを実行する
   戻り値：
     エラーコード
 */
static Long Namests(Long name, Long buf) {
  char nbuf[256];
  int wild = 0;
  const char *name_ptr = GetStringSuper(name);

  Span mem = GetWritableMemorySuper(buf, SIZEOF_NAMESTS);
  if (!mem.bufptr) throwBusErrorOnWrite(buf + mem.length);
  char *buf_ptr = mem.bufptr;
  memset(buf_ptr, 0x00, SIZEOF_NAMESTS);

  int len = strlen(name_ptr);
  if (len > 88) return (-13); /* ファイル名の指定誤り */
  strcpy(nbuf, name_ptr);

  /* 拡張子をセット */
  int i;
  for (i = len - 1; i >= 0 && nbuf[i] != '.'; i--) {
    if (nbuf[i] == '*' || nbuf[i] == '?') wild = 1;
  }
  if (strlen(&(nbuf[i])) > 4) return (-13);
  memset(buf_ptr + 75, ' ', 3);
  if (i < 0) {
    /* 拡張子なし */
    i = len;
  } else {
    memcpy(buf_ptr + 75, &(nbuf[i + 1]), strlen(&(nbuf[i + 1])));
    nbuf[i] = '\0';
  }

  /* ファイル名をセット */
  for (i--; i >= 0; i--) {
    if (nbuf[i] == '\\' || nbuf[i] == '/' || nbuf[i] == ':') break;
    if (nbuf[i] == '*' || nbuf[i] == '?') wild = 1;
  }
  i++;
  if (strlen(&(nbuf[i])) > 18) return (-13);
  if (strlen(&(nbuf[i])) > 8) /* 本当はエラーではない */
    return (-13);
  memset(buf_ptr + 67, ' ', 8);
  memcpy(buf_ptr + 67, &(nbuf[i]), strlen(&(nbuf[i])));
  nbuf[i] = '\0';

  if (wild != 0) mem_set(buf, 0x01, S_BYTE);

  /* パス名をセット */
  if (i == 0) {
    /* カレントディレクトリをセット */
    char cud[67];
    if (Curdir(0, cud) != 0) return DOSE_ILGFNAME;
    strcat(strcpy(buf_ptr + 2, cud), "\\");
  } else {
    for (i--; i >= 0; i--) {
      if (nbuf[i] == ':') break;
    }
    i++;
    if (strlen(&(nbuf[i])) > 64) return (-13);
    strcpy(buf_ptr + 2, &(nbuf[i]));
  }

  /* ドライブ名をセット */
  UByte drv = 0;
  if (isalpha(nbuf[0]) && nbuf[1] == ':') {
    mem_set(buf + 1, toupper(nbuf[0]) - 'A', S_BYTE);
  }
#ifdef _WIN32
  else {
    char path[MAX_PATH];
    GetCurrentDirectory(strlen(path), path);
    UByte drv = path[0] - 'A';
  }
#endif
  mem_set(buf + 1, drv, S_BYTE);

  return (0);
}

/*
 　機能：DOSCALL NAMECKを実行する
 戻り値：エラーコード

 $ff37	_NAMECK		ファイル名の展開

 引数	FILE.l		ファイル名のポインタ
                BUFFER.l	バッファのポインタ

 返値	d0.l = $ff	ファイル指定なし
                d0.l =   0	ワイルドカード指定なし
                d0.l <   0	エラーコード(BUFFER の内容は意味がない)
                d0.l = その他	ワイルドカード指定あり(d0.l
 はワイルドカードの文字数)

        FILE で指定したファイルを、BUFFER で指定した 91
 バイトのバッファに展開する.

 offset	size
 0	  1+1.b	ドライブ名＋':'
 2	 64+1.b	パス名＋0
 67	 18+1.b	ファイル名＋0
 86	1+3+1.b	拡張子('.'＋拡張子＋0)

 */
static Long Nameck(Long name, Long buf) {
  char nbuf[89];
  int ret = 0;
  char *name_ptr = GetStringSuper(name);

  Span mem = GetWritableMemorySuper(buf, SIZEOF_NAMECK);
  if (!mem.bufptr) throwBusErrorOnWrite(buf + mem.length);
  char *buf_ptr = mem.bufptr;
  memset(buf_ptr, 0x00, SIZEOF_NAMECK);

  int len = strlen(name_ptr);
  if (len > 88) return (-13); /* ファイル名の指定誤り */
  strcpy(nbuf, name_ptr);

  /* 拡張子をセット */
  int i;
  for (i = len - 1; i >= 0 && nbuf[i] != '.'; i--) {
    if (nbuf[i] == '*' || nbuf[i] == '?') ret = 1;
  }
  if (strlen(&(nbuf[i])) > 4) return (-13);
  if (i < 0) { /* 拡張子なし */
    i = len;
  } else {
    strcpy(buf_ptr + 86, &(nbuf[i]));
    nbuf[i] = '\0';
  }

  /* ファイル名をセット */
  for (i--; i >= 0; i--) {
    if (nbuf[i] == '\\' || nbuf[i] == '/' || nbuf[i] == ':') break;
    if (nbuf[i] == '*' || nbuf[i] == '?') ret = 1;
  }
  i++;
  if (strlen(&(nbuf[i])) > 18) return (-13);
  strcpy(buf_ptr + 67, &(nbuf[i]));
  nbuf[i] = '\0';

  /* パス名をセット */
  if (i <= 0) {
    strcpy(buf_ptr + 2, ".\\");
  } else {
    for (i--; i >= 0; i--) {
      if (nbuf[i] == ':') break;
    }
    i++;
    if (strlen(&(nbuf[i])) > 64) return (-13);
    strcpy(buf_ptr + 2, &(nbuf[i]));
  }

  /* ドライブ名をセット */
  if (i == 0) {
    /* カレントドライブをセット */
#ifdef _WIN32
    char path[MAX_PATH];
    GetCurrentDirectoryA(sizeof(path), path);
    buf_ptr[0] = path[0];
    buf_ptr[1] = ':';
#else
    Long d;
    dos_getdrive(&d);
    buf_ptr[0] = d - 1 + 'A';
    buf_ptr[1] = ':';
#endif
  } else {
    memcpy(buf_ptr, nbuf, 2);
  }
  //	printf("NAMECK=%s\n", buf_ptr);
  return (ret);
}

/*
 　機能：DOSCALL CONCTRLを実行する
 戻り値：modeによって異なる
 */
static Long Conctrl(short mode, Long adr) {
  short srt;
  short x, y;

  switch (mode) {
    case 0: {
      UWord code = mem_get(adr, S_WORD);
      if (code >= 0x0100) putchar(code >> 8);
      putchar(code & 0xff);
#ifdef _WIN32
      FlushFileBuffers(finfo[1].host.handle);
#else
      fflush(stdout);
#endif
    } break;
    case 1:
      printf("%s", GetStringSuper(mem_get(adr, S_LONG)));
      break;
    case 2: /* 属性 */
      srt = (short)mem_get(adr, S_WORD);
      text_color(srt);
      break;
    case 3: /* locate */
      x = (short)mem_get(adr, S_WORD);
      y = (short)mem_get(adr + 2, S_WORD);
      printf("%c[%d;%dH", 0x1B, y + 1, x + 1);
      break;
    case 4: /* １行下にカーソル移動(スクロール有り) */
      printf("%c[s\n%c[u%c[1B", 0x1B, 0x1B, 0x1B);
      break;
    case 5: /* １行上にカーソル移動(スクロール未サポート) */
      printf("%c[1A", 0x1B);
      break;
    case 6: /* srt行上にカーソル移動 */
      srt = (short)mem_get(adr, S_WORD);
      printf("%c[%dA", 0x1B, srt);
      break;
    case 7: /* srt行下にカーソル移動 */
      srt = (short)mem_get(adr, S_WORD);
      printf("%c[%dB", 0x1B, srt);
      break;
    case 8: /* srt文字右にカーソル移動 */
      srt = (short)mem_get(adr, S_WORD);
      printf("%c[%dC", 0x1B, srt);
      break;
    case 9: /* srt文字左にカーソル移動 */
      srt = (short)mem_get(adr, S_WORD);
      printf("%c[%dD", 0x1B, srt);
      break;
    case 10:
      srt = (short)mem_get(adr, S_WORD);
      switch (srt) {
        case 0: /* 最終行左端まで消去 */
          printf("%c[0J", 0x1B);
          break;
        case 1: /* ホームからカーソル位置まで消去 */
          printf("%c[1J", 0x1B);
          break;
        case 2: /* 画面を消去 */
          printf("%c[2J", 0x1B);
          break;
      }
      break;
    case 11:
      srt = (short)mem_get(adr, S_WORD);
      switch (srt) {
        case 0: /* 右端まで消去 */
          printf("%c[K", 0x1B);
          break;
        case 1: /* 左端からカーソル位置まで消去 */
          printf("%c[1K", 0x1B);
          break;
        case 2:                    /* 1行消去 */
          printf("%c[s", 0x1B);    /* 位置保存 */
          printf("%c[999D", 0x1B); /* 左端に移動 */
          printf("%c[K", 0x1B);    /* 右端まで消去 */
          printf("%c[u", 0x1B);    /* 位置再設定 */
          break;
      }
      break;
    case 12: /* カーソル行にsrt行挿入 */
      srt = (short)mem_get(adr, S_WORD);
      printf("%c[%dL", 0x1B, srt);
      break;
    case 13: /* カーソル行からsrt行削除 */
      srt = (short)mem_get(adr, S_WORD);
      printf("%c[%dM", 0x1B, srt);
      break;
    case 17: /* カーソル表示 */
      printf("%c[>5l", 0x1B);
      break;
    case 18: /* カーソル消去 */
      printf("%c[>5h", 0x1B);
      break;
  }

  return (0);
}

/*
 　機能：DOSCALL KEYCTRLを実行する
 戻り値：キーコード等(modeによって異なる)
 */
static Long Keyctrl(short mode, Long stack_adr) {
  UByte c;

  switch (mode) {
    case 0:
      c = _getch();
      if (c == 0x00) {
        c = _getch();
        if (c == 0x85) /* F11 */
          c = 0x03;    /* break */
      }
      return (c);
#ifdef _WIN32
    case 1: /* キーの先読み */
      if (_kbhit() == 0) return (0);
      c = _getch();
      if (c == 0x00) {
        c = _getch();
        if (c == 0x85) /* F11 */
          c = 0x03;    /* break */
      }
      _ungetch(c);
      return (c);
#endif
    default:
      return (0);
  }
}

/*
 　機能：DOSCALL FNCKEYを実行する
 戻り値：なし
 */
static void Fnckey(short mode, Long buf) {
  int fno = mode & 0xff;
  ULong len = (fno >= 21) ? 6 : (fno >= 1) ? 32 : 712;

  if ((mode & 0xff00) == 0) {
    Span mem = GetWritableMemorySuper(buf, len);
    if (!mem.bufptr) throwBusErrorOnWrite(buf + mem.length);
    get_fnckey(fno, mem.bufptr);
  } else {
    Span mem = GetReadableMemorySuper(buf, len);
    if (!mem.bufptr) throwBusErrorOnRead(buf + mem.length);
    put_fnckey(fno, mem.bufptr);
  }
}

/*
 　機能：DOSCALL INTVCGを実行する
 戻り値：ベクタの値
 */
static Long Intvcg(UWord intno) {
  Long adr2;
  Long mae;
  short save_s;

  if (intno >= 0xFF00) { /* DOSCALL */
    intno &= 0xFF;
    adr2 = 0x1800 + intno * 4;
    save_s = SR_S_REF();
    SR_S_ON();
    mae = mem_get(adr2, S_LONG);
    if (save_s == 0) SR_S_OFF();
    return (mae);
  }

  intno &= 0xFF;
  adr2 = intno * 4;
  save_s = SR_S_REF();
  SR_S_ON();
  mae = mem_get(adr2, S_LONG);
  if (save_s == 0) SR_S_OFF();
  return (mae);
}

/*
 　機能：DOSCALL INTVCSを実行する
 戻り値：設定前のベクタ
 */
static Long Intvcs(UWord intno, Long adr) {
  Long adr2;
  Long mae;
  short save_s;

  if (intno >= 0xFF00) { /* DOSCALL */
    intno &= 0xFF;
    adr2 = 0x1800 + intno * 4;
    save_s = SR_S_REF();
    SR_S_ON();
    mae = mem_get(adr2, S_LONG);
    mem_set(adr2, adr, S_LONG);
    if (save_s == 0) SR_S_OFF();
    return (mae);
  }

  return (0);
}

/*
 　機能：DOSCALL ASSIGNを実行する
 戻り値：エラーコード他
 */
static Long Assign(short mode, Long stack_adr) {
  Long drv = mem_get(stack_adr, S_LONG);
  Long buf = mem_get(stack_adr + 4, S_LONG);

  if (mode == 0) {
    const char *drv_ptr = GetStringSuper(drv);

    if (drv_ptr[1] != ':' || drv_ptr[2] != '\0') return DOSE_ILGPARM;
    short d = toupper(drv_ptr[0]) - 'A' + 1;
    if (d < 1 || d > 26) return DOSE_ILGPARM;

    char dir[HUMAN68K_DIR_MAX + 1];
    if (Curdir(d, dir) != 0) return DOSE_ILGPARM;
    WriteStringSuper(buf, dir);
    return 0x40;
  }

  return DOSE_ILGPARM;
}

/*
 　機能：DOSCALL GETFCBを実行する
 戻り値：FCBのアドレス
 */
static Long Getfcb(short fhdl) {
  static unsigned char fcb[4][0x60] = {
      {0x01, 0xC1, 0x00, 0x02, 0xC6, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x43, 0x4F, 0x4E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x01, 0xC2, 0x00, 0x02, 0xC6, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x43, 0x4F, 0x4E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x01, 0xC2, 0x00, 0x02, 0xC6, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x43, 0x4F, 0x4E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

  ULong adr = FCB_WORK;
  ULong len = 0x60;

  Span mem = GetWritableMemorySuper(adr, len);
  if (!mem.bufptr) throwBusErrorOnWrite(adr + mem.length);

  switch (fhdl) {
    case 0:
    case 1:
    case 2:
      memcpy(mem.bufptr, fcb[fhdl], len);
      return adr;
    default:
      fcb[3][14] = (unsigned char)fhdl;
      memcpy(mem.bufptr, fcb[3], len);
      return adr;
  }
}

// DOS _EXECの引数(実行ファイル名)からファイルタイプを抽出する。
//   実行ファイル名の最上位バイトをクリアする。
static bool getExecType(ULong *refFile, ExecType *outType) {
  UByte t = *refFile >> 24;
  *outType = EXEC_TYPE_DEFAULT;

  if (settings.highMemorySize) {
    // ハイメモリ有効時はファイルタイプ指定不可としている。
    // ただし正しく検出できない場合もある。
    return ((EXEC_TYPE_R <= t) && (t <= EXEC_TYPE_X)) ? false : true;
  }

  if (t <= EXEC_TYPE_X) *outType = t;
  *refFile &= ADDRESS_MASK;
  return true;
}

/*
 　機能：DOSCALL EXEC(mode=0,1)を実行する
 戻り値：エラーコード等
 */
static Long Exec01(ULong nm, Long cmd, Long env, int md) {
  FILE *fp;
  char fname[89];

  ExecType execType;
  if (!getExecType(&nm, &execType)) return DOSE_ILGPARM;

  char *name_ptr = GetStringSuper(nm);
  if (strlen(name_ptr) > 88) return DOSE_ILGFNAME;  // ファイル名指定誤り

  strcpy(fname, name_ptr);
  if ((fp = prog_open(fname, env, NULL)) == NULL) return (-2);

  Human68kPathName hpn;
  if (!HOST_CANONICAL_PATHNAME(fname, &hpn)) return DOSE_ILGFNAME;

  if (nest_cnt + 1 >= NEST_MAX) return (-8);

  // 最大メモリを確保する
  ULong parentPsp = psp[nest_cnt];
  MallocResult child = MallocAll(parentPsp);
  if (child.address < 0) {
    fclose(fp);
    return DOSE_NOMEM;  // メモリが確保できない
  }
  Long childPsp = child.address - SIZEOF_MEMBLK;

  Long end_adr = mem_get(childPsp + MEMBLK_END, S_LONG);
  Long prog_size = 0;
  Long prog_size2 = end_adr;
  const Long entryAddress = prog_read(fp, fname, childPsp + SIZEOF_PSP,
                                      &prog_size, &prog_size2, NULL, execType);
  if (entryAddress < 0) {
    Mfree(child.address);
    return entryAddress;
  }

  ULong envptr =
      (env == 0) ? mem_get(psp[nest_cnt] + PSP_ENV_PTR, S_LONG) : env;

  const ProgramSpec progSpec = {prog_size2, prog_size - prog_size2};
  BuildPsp(childPsp, envptr, cmd, sr, parentPsp, &progSpec, &hpn);

  nest_pc[nest_cnt] = pc;
  nest_sp[nest_cnt] = ra[7];
  ra[0] = childPsp;
  ra[1] = childPsp + SIZEOF_PSP + prog_size;
  ra[2] = cmd;
  ra[3] = envptr;
  ra[4] = entryAddress;
  nest_cnt++;
  psp[nest_cnt] = childPsp;

  if (md == 0) {
    pc = ra[4];
    return rd[0];
  }
  nest_cnt--;
  return ra[4];
}

/*
 　機能：DOSCALL EXEC(mode=2)を実行する
 戻り値：エラーコード
 */
static Long Exec2(Long nm, Long cmd, Long env) {
  char *name_ptr = GetStringSuper(nm);
  char *cmd_ptr = GetStringSuper(cmd);

  char *p = name_ptr;
  while (*p != '\0' && *p != ' ') p++;
  if (*p != '\0') { /* コマンドラインあり */
    *p = '\0';
    p++;
    *cmd_ptr = strlen(p);
    strcpy(cmd_ptr + 1, p);
  }

  /* 環境変数pathに従ってファイルを検索し、オープンする。*/
  FILE *fp = prog_open(name_ptr, env, print);
  if (fp == NULL) {
    return 0;
  } else {
    fclose(fp);
  }
  return (0);
}

/*
 　機能：DOSCALL EXEC(mode=3)を実行する
 戻り値：エラーコード等
 */
static Long Exec3(ULong nm, Long adr1, Long adr2) {
  char fname[89];

  ExecType execType;
  if (!getExecType(&nm, &execType)) return DOSE_ILGPARM;

  char *name_ptr = GetStringSuper(nm);
  if (strlen(name_ptr) > 88) return (-13); /* ファイル名指定誤り */

  strcpy(fname, name_ptr);
  FILE *fp = prog_open(fname, (ULong)-1, NULL);
  if (fp == NULL) return -2;

  Long prog_size;
  Long prog_size2 = adr2;
  Long ret =
      prog_read(fp, fname, adr1, &prog_size, &prog_size2, NULL, execType);
  if (ret < 0) return (ret);

  return (prog_size);
}

/*
 　機能：DOSCALL EXEC(mode=4)を実行する
 戻り値：エラーコード等
 */
static void Exec4(Long adr) {
  nest_pc[nest_cnt] = pc;
  nest_sp[nest_cnt] = ra[7];
  nest_cnt++;
  pc = adr;
}

/*
 　機能：getsの代わりをする
 戻り値：なし
 */
Long gets2(char *str, int max) {
  int c;
  int cnt;

  cnt = 0;
  c = getchar();
  if (c == EOF) fseek(stdin, 0, 0);
  while (c != EOF && c != '\n') {
    if (cnt < max) str[cnt++] = c;
    c = getchar();
  }
  if (c == EOF) str[cnt++] = EOF;
#ifdef _WIN32
  unsigned dmy;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "\x01B[1A", 4, &dmy, NULL);
#endif
  /* printf("%c[1A", 0x1B); */ /* カーソルを１行上に */
  str[cnt] = '\0';

  return (strlen(str));
}

/* $Id: doscall.c,v 1.3 2009/08/08 06:49:44 masamic Exp $ */

/*
 * $Log: doscall.c,v $
 * Revision 1.3  2009/08/08 06:49:44  masamic
 * Convert Character Encoding Shifted-JIS to UTF-8.
 *
 * Revision 1.2  2009/08/05 14:44:33  masamic
 * Some Bug fix, and implemented some instruction
 * Following Modification contributed by TRAP.
 *
 * Fixed Bug: In disassemble.c, shift/rotate as{lr},ls{lr},ro{lr} alway show
 * word size.
 * Modify: enable KEYSNS, register behaiviour of sub ea, Dn.
 * Add: Nbcd, Sbcd.
 *
 * Revision 1.1.1.1  2001/05/23 11:22:07  masamic
 * First imported source code and docs
 *
 * Revision 1.14  2000/01/19  03:51:39  yfujii
 * NFILES is debugged.
 *
 * Revision 1.13  2000/01/16  05:38:53  yfujii
 * Function call 'NAMESTS' is debugged.
 *
 * Revision 1.12  2000/01/09  04:24:13  yfujii
 * Func call CURDRV is fixed according to buginfo0002.
 *
 * Revision 1.11  1999/12/23  08:06:27  yfujii
 * Bugs of FILES/NFILES calls are fixed.
 *
 * Revision 1.10  1999/12/07  12:41:55  yfujii
 * *** empty log message ***
 *
 * Revision 1.10  1999/11/29  07:57:04  yfujii
 * Modified time/date retrieving code to be correct.
 *
 * Revision 1.7  1999/10/29  13:44:11  yfujii
 * FGETC function is debugged.
 *
 * Revision 1.6  1999/10/26  12:26:08  yfujii
 * Environment variable function is drasticaly modified.
 *
 * Revision 1.5  1999/10/25  03:23:59  yfujii
 * Some function calls are modified to be correct.
 *
 * Revision 1.4  1999/10/21  04:08:21  yfujii
 * A lot of warnings are removed.
 *
 * Revision 1.3  1999/10/20  06:00:06  yfujii
 * Compile time errors are all removed.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN/32 a little.
 *
 */
