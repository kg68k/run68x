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
#include "dos_memory.h"
#include "host_generic.h"
#include "host_win32.h"
#include "human68k.h"
#include "mem.h"
#include "run68.h"

static Long Gets(Long);
static Long Kflush(short);
static Long Ioctrl(short, Long);
static Long Dup(short);
static Long Dup2(short, short);
static Long Dskfre(short, Long);
static Long Create(char *, short);
static Long Newfile(char *, short);
static Long Open(char *, short);
static Long Close(short);
static Long Fgets(Long, short);
static Long Read(short, Long, Long);
#if defined(__APPLE__) || defined(__linux__) || defined(__EMSCRIPTEN__)
static Long Read_conv(short, void *, size_t);
#endif
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
static Long Getenv(Long, Long, Long);
static Long Namests(Long, Long);
static Long Nameck(Long, Long);
static Long Conctrl(short, Long);
static Long Keyctrl(short, Long);
static void Fnckey(short, Long);
static Long Intvcg(UWord);
static Long Intvcs(UWord, Long);
static Long Assign(short, Long);
static Long Getfcb(short);
static Long Exec01(Long, Long, Long, int);
static Long Exec2(Long, Long, Long);
static Long Exec3(Long, Long, Long);
static void Exec4(Long);
static Long gets2(char *, int);

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

// DOS _FILEDATE (0xff57, 0xff87)
static Long DosFiledate(ULong param) {
  UWord fileno = ReadParamUWord(&param);
  ULong dt = ReadParamULong(&param);
  return HOST_DOS_FILEDATE(fileno, dt);
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
static bool Exit2(const char *name, Long exit_code) {
  if (func_trace_f) {
    printf("%-10s\n", name);
  }
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

// DOSコール名を表示する
//   function call trace用
static void print_doscall_name(const char *name) { printf("%-10s ", name); }

// DOSコールの引数のドライブ名を表示する
//   function call trace用
static void print_drive_param(const char *prefix, UWord drive,
                              const char *suffix) {
  printf("%s", prefix);
  if (drive == 0) {
    printf("current drive");
  } else if (1 <= drive && drive <= 26) {
    printf("%c:\n", (drive - 1) + 'A');
    return;
  } else {
    printf("%d(\?\?))", drive);
  }
  printf("%s", suffix);
}

/*
 　機能：DOSCALLを実行する
 戻り値： true = 実行終了
         false = 実行継続
 */
bool dos_call(UByte code) {
  char *data_ptr = 0;
  Long stack_adr;
  Long data;
  Long env;
  Long buf;
  Long len;
  short srt;
  short fhdl;
  Long c = 0;
  int i;
#ifdef _WIN32
  DWORD st;
#endif
  if (func_trace_f) {
    printf("$%06x FUNC(%02X):", pc - 2, code);
  }
  stack_adr = ra[7];
  if (code >= 0x80 && code <= 0xAF) code -= 0x30;

#ifdef TRACE
  printf("trace: DOSCALL  0xFF%02X PC=%06lX\n", code, pc);
#endif

  switch (code) {
    case 0x01: /* GETCHAR */
      if (func_trace_f) {
        printf("%-10s\n", "GETCHAR");
      }
#ifdef _WIN32
      FlushFileBuffers(finfo[1].host.handle);
#endif
      rd[0] = (_getche() & 0xFF);
      break;
    case 0x02:                              /* PUTCHAR */
      data_ptr = prog_ptr + stack_adr + 1;  // mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s char='%c'\n", "PUTCHAR", *(unsigned char *)data_ptr);
      }
#ifdef _WIN32
      {
        FILEINFO *finfop = &finfo[1];
        if (GetConsoleMode(finfop->host.handle, &st) != 0) {
          // 非リダイレクト
          WriteW32(1, finfop->host.handle, data_ptr, 1);
        } else {
          Long nwritten;
          /* Win32API */
          WriteFile(finfop->host.handle, data_ptr, 1, (LPDWORD)&nwritten, NULL);
        }
      }
#else
      Write_conv(1, data_ptr, 1);
#endif
      rd[0] = 0;
      break;
    case 0x06: /* KBHIT */
      if (func_trace_f) {
        printf("%-10s\n", "KBHIT");
      }
      srt = (short)mem_get(stack_adr, S_WORD);
      srt &= 0xFF;
      if (srt >= 0xFE) {
#ifdef _WIN32
        FILEINFO *finfop = &finfo[0];
        INPUT_RECORD ir;
        DWORD read_len = 0;
        rd[0] = 0;
        PeekConsoleInput(finfop->host.handle, &ir, 1, (LPDWORD)&read_len);
        if (read_len == 0) {
          /* Do nothing. */
        } else if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown ||
                   c == 0x0) {
          /* 不要なイベントは読み捨てる */
          ReadConsoleInput(finfop->host.handle, &ir, 1, (LPDWORD)&read_len);
        } else if (srt != 0xFE) {
          ReadConsoleInput(finfop->host.handle, &ir, 1, (LPDWORD)&read_len);
          c = ir.Event.KeyEvent.uChar.AsciiChar;
          if (ini_info.pc98_key) c = cnv_key98(c);
          rd[0] = c;
        }
#else
        rd[0] = 0;
        if (kbhit() != 0) {
          c = _getch();
          if (c == 0x00) {
            c = _getch();
          } else {
            if (ini_info.pc98_key) c = cnv_key98(c);
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
      if (func_trace_f) {
        printf("%-10s\n", code == 0x07 ? "INKEY" : "GETC");
      }
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
      data = mem_get(stack_adr, S_LONG);
      data_ptr = prog_ptr + data;
      len = strlen(data_ptr);
      if (func_trace_f) {
        printf("%-10s str=%s\n", "PRINT", data_ptr);
      }
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
      /* printf( "%s", data_ptr ); */
      rd[0] = 0;
      break;
    case 0x0A: /* GETS */
      buf = mem_get(stack_adr, S_LONG);
      if (func_trace_f) {
        printf("%-10s\n", "GETS");
      }
      rd[0] = Gets(buf);
      break;
    case 0x0B: /* KEYSNS */
      if (func_trace_f) {
        printf("%-10s\n", "KEYSNS");
      }
      if (_kbhit() != 0)
        rd[0] = -1;
      else
        rd[0] = 0;
      break;
    case 0x0C: /* KFLUSH */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s mode=%d\n", "KFLUSH", srt);
      }
      rd[0] = Kflush(srt);
      break;
    case 0x0D: /* FFLUSH */
      if (func_trace_f) {
        printf("%-10s\n", "FFLUSH");
      }
#ifdef _WIN32
      /* オープン中の全てのファイルをフラッシュする。*/
      for (i = 5; i < FILE_MAX; i++) {
        if (finfo[i].is_opened) FlushFileBuffers(finfo[i].host.handle);
      }
#else
      _flushall();
#endif
      rd[0] = 0;
      break;
    case 0x0E: /* CHGDRV */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s drv=%c:\n", "CHGDRV", srt + 'A');
      }
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
      if (func_trace_f) {
        printf("%-10s drv=%c:\n", "DRVCTRL", srt);
      }
      if (srt > 26 && srt < 256)
        rd[0] = -15; /* ドライブ指定誤り */
      else
        rd[0] = 0x02; /* READY */
      break;
    case 0x10: /* CONSNS */
      if (func_trace_f) {
        printf("%-10s\n", "CONSNS");
      }
      _flushall();
      rd[0] = -1;
      break;
    case 0x11: /* PRNSNS */
    case 0x12: /* CINSNS */
    case 0x13: /* COUTSNS */
      if (func_trace_f) {
        printf("%-10s\n", code == 0x11   ? "PRNSNS"
                          : code == 0x12 ? "CINSNS"
                                         : "COUTSNS");
      }
      _flushall();
      rd[0] = 0;
      break;
    case 0x19: /* CURDRV */
      if (func_trace_f) {
        printf("%-10s\n", "CURDRV");
      }
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
      if (func_trace_f) {
        printf("%-10s\n", "FGETC");
      }
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
      if (func_trace_f) {
        printf("%-10s file_no=%d\n", "FGETS", fhdl);
      }
      rd[0] = Fgets(data, fhdl);
      break;
    case 0x1D:                              /* FPUTC */
      data_ptr = prog_ptr + stack_adr + 1;  // mem_get(stack_adr, S_WORD);
      fhdl = (short)mem_get(stack_adr + 2, S_WORD);
      if (func_trace_f) {
        printf("%-10s file_no=%d char=0x%02X\n", "FPUTC", fhdl,
               *(unsigned char *)data_ptr);
      }
#ifdef _WIN32
      {
        FILEINFO *finfop = &finfo[fhdl];
        if (GetConsoleMode(finfop->host.handle, &st) != 0 &&
            (fhdl == 1 || fhdl == 2)) {
          // 非リダイレクトで標準出力か標準エラー出力
          WriteW32(fhdl, finfop->host.handle, data_ptr, 1);
          rd[0] = 0;
        } else {
          if (WriteFile(finfop->host.handle, data_ptr, 1, (LPDWORD)&len,
                        NULL) == FALSE)
            rd[0] = 0;
          else
            rd[0] = 1;
        }
      }
#else
      if (Write_conv(fhdl, data_ptr, 1) == EOF)
        rd[0] = 0;
      else
        rd[0] = 1;
#endif
      break;
    case 0x1E: /* FPUTS */
      data = mem_get(stack_adr, S_LONG);
      fhdl = (short)mem_get(stack_adr + 4, S_WORD);
      data_ptr = prog_ptr + data;
      if (func_trace_f) {
        printf("%-10s file_no=%d str=\"%s\"\n", "FPUTS", fhdl, data_ptr);
      }
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
      if (func_trace_f) {
        printf("%-10s\n", "ALLCLOSE");
      }
      close_all_files();
      rd[0] = 0;
      break;
    case 0x20: /* SUPER */
      data = mem_get(stack_adr, S_LONG);
      if (func_trace_f) {
        printf("%-10s data=%d\n", "SUPER", data);
      }
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
      if (func_trace_f) {
        printf("%-10s mode=%d\n", "FNCKEY", srt);
      }
      Fnckey(srt, buf);
      rd[0] = 0;
      break;
    case 0x23: /* CONCTRL */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s mode=%d\n", "CONCTRL", srt);
      }
      rd[0] = Conctrl(srt, stack_adr + 2);
      break;
    case 0x24: /* KEYCTRL */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s mode=%d\n", "KEYCTRL", srt);
      }
      rd[0] = Keyctrl(srt, stack_adr + 2);
      break;
    case 0x25: /* INTVCS */
      srt = (short)mem_get(stack_adr, S_WORD);
      data = mem_get(stack_adr + 2, S_LONG);
      if (func_trace_f) {
        printf("%-10s intno=%d vec=%X\n", "INTVCS", srt, data);
      }
      rd[0] = Intvcs(srt, data);
      break;
    case 0x27: /* GETTIM2 */
      if (func_trace_f) {
        printf("%-10s\n", "GETTIM2");
      }
      rd[0] = Gettime(1);
      break;
    case 0x28: /* SETTIM2 */
      data = mem_get(stack_adr, S_LONG);
      if (func_trace_f) {
        printf("%-10s %X\n", "SETTIM2", data);
      }
      rd[0] = Settim2(data);
      break;
    case 0x29: /* NAMESTS */
      data = mem_get(stack_adr, S_LONG);
      buf = mem_get(stack_adr + 4, S_LONG);
      if (func_trace_f) {
        printf("%-10s fname=%s\n", "NAMESTS", prog_ptr + data);
      }
      rd[0] = Namests(data, buf);
      break;
    case 0x2A: /* GETDATE */
      if (func_trace_f) {
        printf("%-10s", "GETDATE");
      }
      rd[0] = Getdate();
      if (func_trace_f) {
        printf("date=%X\n", rd[0]);
      }
      break;
    case 0x2B: /* SETDATE */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s date=%X\n", "SETDATE", srt);
      }
      rd[0] = Setdate(srt);
      break;
    case 0x2C: /* GETTIME */
      if (func_trace_f) {
        printf("%-10s flag=0\n", "GETTIME");
      }
      rd[0] = Gettime(0);
      break;
    case 0x30: /* VERNUM */
      if (func_trace_f) {
        printf("%-10s\n", "VERNUM");
      }
      rd[0] = 0x36380302;
      break;
    case 0x32: /* GETDPB */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s: drive=%d. Not supported, returns -1.\n", "GETDPB", srt);
      }
      rd[0] = -1;
      break;
    case 0x33: /* BREAKCK */
      if (func_trace_f) {
        printf("%-10s\n", "BREAKCK");
      }
      rd[0] = 1;
      break;
    case 0x34: /* DRVXCHG */
      if (func_trace_f) {
        printf("%-10s\n", "DRVXCHG");
      }
      rd[0] = -15; /* ドライブ指定誤り */
      break;
    case 0x35: /* INTVCG */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s intno=%d", "INTVCG", srt);
      }
      rd[0] = Intvcg(srt);
      if (func_trace_f) {
        printf(" vec=%X\n", rd[0]);
      }
      break;
    case 0x36: /* DSKFRE */
      srt = (short)mem_get(stack_adr, S_WORD);
      buf = mem_get(stack_adr + 2, S_LONG);
      if (func_trace_f) {
        printf("%-10s drv=%c:\n", "DISKFRE", srt);
      }
      rd[0] = Dskfre(srt, buf);
      break;
    case 0x37: /* NAMECK */
      data = mem_get(stack_adr, S_LONG);
      buf = mem_get(stack_adr + 4, S_LONG);
      if (func_trace_f) {
        printf("%-10s fname=%s\n", "NAMECK", prog_ptr + data);
      }
      rd[0] = Nameck(data, buf);
      break;
    case 0x39: /* MKDIR */
      data = mem_get(stack_adr, S_LONG);
      if (func_trace_f) {
        printf("%-10s dname=%s\n", "MKDIR", prog_ptr + data);
      }
      rd[0] = Mkdir(data);
      break;
    case 0x3A: /* RMDIR */
      data = mem_get(stack_adr, S_LONG);
      if (func_trace_f) {
        printf("%-10s dname=%s\n", "RMDIR", prog_ptr + data);
      }
      rd[0] = Rmdir(data);
      break;
    case 0x3B: /* CHDIR */
      data = mem_get(stack_adr, S_LONG);
      if (func_trace_f) {
        printf("%-10s dname=%s\n", "CHDIR", prog_ptr + data);
      }
      rd[0] = Chdir(data);
      break;
    case 0x3C: /* CREATE */
      data = mem_get(stack_adr, S_LONG);
      srt = (short)mem_get(stack_adr + 4, S_WORD);
      data_ptr = prog_ptr + data;
      if (func_trace_f) {
        printf("%-10s fname=%s attr=%d\n", "CREATE", data_ptr, srt);
      }
      rd[0] = Create(data_ptr, srt);
      break;
    case 0x3D: /* OPEN */
      data = mem_get(stack_adr, S_LONG);
      srt = (short)mem_get(stack_adr + 4, S_WORD);
      data_ptr = prog_ptr + data;
      if (func_trace_f) {
        printf("%-10s fname=%s mode=%d\n", "OPEN", data_ptr, srt);
      }
      rd[0] = Open(data_ptr, srt);
      break;
    case 0x3E: /* CLOSE */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s file_no=%d\n", "CLOSE", srt);
      }
      rd[0] = Close(srt);
      break;
    case 0x3F: /* READ */
      srt = (short)mem_get(stack_adr, S_WORD);
      data = mem_get(stack_adr + 2, S_LONG);
      len = mem_get(stack_adr + 6, S_LONG);
      rd[0] = Read(srt, data, len);
      if (func_trace_f) {
        char *str = prog_ptr + data;
        printf("%-10s file_no=%d size=%d ret=%d str=", "READ", srt, len, rd[0]);
        for (i = 0; i < (len <= 30 ? len : 30); i++) {
          if (str[i] == 0) break;
          if (str[i] < ' ') printf("\\%03o", (unsigned char)str[i]);
          putchar(str[i]);
        }
        if (len > 30) printf(" ...(truncated)");
        printf("\n");
      }
      break;
    case 0x40: /* WRITE */
      srt = (short)mem_get(stack_adr, S_WORD);
      data = mem_get(stack_adr + 2, S_LONG);
      len = mem_get(stack_adr + 6, S_LONG);
      rd[0] = Write(srt, data, len);
      if (func_trace_f) {
        char *str = prog_ptr + data;
        printf("%-10s file_no=%d size=%d ret=%d str=", "WRITE", srt, len,
               rd[0]);
        for (i = 0; i < (len <= 30 ? len : 30); i++) {
          if (str[i] == 0) break;
          if (str[i] < ' ') printf("\\%03o", (unsigned char)str[i]);
          putchar(str[i]);
        }
        if (len > 30) printf(" ...(truncated)");
        printf("\n");
      }
      break;
    case 0x41: /* DELETE */
      data = mem_get(stack_adr, S_LONG);
      data_ptr = prog_ptr + data;
      if (func_trace_f) {
        printf("%-10s fname=%s\n", "DELETE", data_ptr);
      }
      rd[0] = Delete(data_ptr);
      break;
    case 0x42: /* SEEK */
      fhdl = (short)mem_get(stack_adr, S_WORD);
      data = mem_get(stack_adr + 2, S_LONG);
      srt = (short)mem_get(stack_adr + 6, S_WORD);
      rd[0] = Seek(fhdl, data, srt);
      if (func_trace_f) {
        printf("%-10s file_no=%d offset=%d mode=%d ret=%d\n", "SEEK", fhdl,
               data, srt, rd[0]);
      }
      break;
    case 0x43: /* CHMOD */
      data = mem_get(stack_adr, S_LONG);
      srt = (short)mem_get(stack_adr + 4, S_WORD);
      if (func_trace_f) {
        printf("%-10s name=%s attr=%02X\n", "CHMOD", prog_ptr + data, srt);
      }
      rd[0] = Chmod(data, srt);
      break;
    case 0x44: /* IOCTRL */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s mode=%d stack=%08X\n", "IOCTRL", srt, stack_adr + 2);
      }
      rd[0] = Ioctrl(srt, stack_adr + 2);
      break;
    case 0x45: /* DUP */
      fhdl = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s org-handle=%d\n", "DUP", fhdl);
      }
      rd[0] = Dup(fhdl);
      break;
    case 0x46: /* DUP2 */
      srt = (short)mem_get(stack_adr, S_WORD);
      fhdl = (short)mem_get(stack_adr + 2, S_WORD);
      if (func_trace_f) {
        printf("%-10s org-handle=%d new-handle=%d\n", "DUP", srt, fhdl);
      }
      rd[0] = Dup2(srt, fhdl);
      break;
    case 0x47: /* CURDIR */
      srt = (short)mem_get(stack_adr, S_WORD);
      data = mem_get(stack_adr + 2, S_LONG);
      if (func_trace_f) {
        print_doscall_name("CURDIR");
        print_drive_param("drv=", srt, "\n");
      }
      rd[0] = Curdir(srt, prog_ptr + data);
      break;
    case 0x48: /* MALLOC */
      if (func_trace_f) {
        printf("%-10s len=%d\n", "MALLOC", mem_get(stack_adr, S_LONG));
      }
      rd[0] = DosMalloc(stack_adr);
      break;
    case 0x49: /* MFREE */
      if (func_trace_f) {
        printf("%-10s addr=%08X\n", "MFREE", mem_get(stack_adr, S_LONG));
      }
      rd[0] = DosMfree(stack_adr);
      break;
    case 0x4A: /* SETBLOCK */
      if (func_trace_f) {
        printf("%-10s size=%d\n", "SETBLOCK", mem_get(stack_adr + 4, S_LONG));
      }
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
      if (func_trace_f) {
        printf("%-10s md=%d cmd=%s\n", "EXEC", srt, prog_ptr + data);
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
      if (func_trace_f) {
        printf("%-10s fname=\"%s\" attr=%02X\n", "FILES", prog_ptr + data, srt);
      }
      rd[0] = Files(buf, data, srt);
      break;
    case 0x4F: /* NFILES */
      buf = mem_get(stack_adr, S_LONG);
      if (func_trace_f) {
        printf("%-10s\n", "NFILES");
      }
      rd[0] = Nfiles(buf);
      break;
    case 0x51: /* GETPDB */
      rd[0] = psp[nest_cnt] + SIZEOF_MEMBLK;
      if (func_trace_f) {
        printf("%-10s\n", "GETPDB");
      }
      break;
    case 0x53: /* GETENV */
      data = mem_get(stack_adr, S_LONG);
      env = mem_get(stack_adr + 4, S_LONG);
      buf = mem_get(stack_adr + 8, S_LONG);
      if (func_trace_f) {
        printf("%-10s env=%s\n", "GETENV", prog_ptr + data);
      }
      rd[0] = Getenv(data, env, buf);
      break;
    case 0x54: /* VERIFYG */
      if (func_trace_f) {
        printf("%-10s\n", "VERIFYG");
      }
      rd[0] = 1;
      break;
    case 0x56: /* RENAME */
      data = mem_get(stack_adr, S_LONG);
      buf = mem_get(stack_adr + 4, S_LONG);
      if (func_trace_f) {
        printf("%-10s old=\"%s\" new=\"%s\"\n", "RENAME", prog_ptr + data,
               prog_ptr + buf);
      }
      rd[0] = Rename(data, buf);
      break;
    case 0x57: /* FILEDATE */
    case 0x87: /* FILEDATE (Human68k v3) */
      if (func_trace_f) {
        printf("%-10s file_no=%d datetime=%X\n", "FILEDATE",
               mem_get(stack_adr, S_WORD), mem_get(stack_adr + 2, S_LONG));
      }
      rd[0] = DosFiledate(stack_adr);
      break;
    case 0x58: /* MALLOC2 */
    case 0x88: /* MALLOC2 (Human68k v3) */
      if (func_trace_f) {
        printf("%-10s mode=%d, len=%d\n", "MALLOC2", mem_get(stack_adr, S_WORD),
               mem_get(stack_adr + 2, S_LONG));
      }
      rd[0] = DosMalloc2(stack_adr);
      break;
    case 0x5B: /* NEWFILE */
      data = mem_get(stack_adr, S_LONG);
      srt = (short)mem_get(stack_adr + 4, S_WORD);
      data_ptr = prog_ptr + data;
      if (func_trace_f) {
        printf("%-10s name=\"%s\" attr=%d\n", "NEWFILE", data_ptr, srt);
      }
      rd[0] = Newfile(data_ptr, srt);
      break;
    case 0x5F: /* ASSIGN */
      srt = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s mode=%d", "ASSIGN", srt);
      }
      rd[0] = Assign(srt, stack_adr + 2);
      break;
    case 0x7C: /* GETFCB */
      fhdl = (short)mem_get(stack_adr, S_WORD);
      if (func_trace_f) {
        printf("%-10s file_no=%d\n", "GETFCB", fhdl);
      }
      rd[0] = Getfcb(fhdl);
      break;
    case 0xF6: /* SUPER_JSR */
      data = mem_get(stack_adr, S_LONG);
      if (func_trace_f) {
        printf("%-10s adr=$%08X\n", "SUPER_JSR", data);
      }
      ra[7] -= 4;
      mem_set(ra[7], pc, S_LONG);
      if (SR_S_REF() == 0) {
        superjsr_ret = pc;
        SR_S_ON();
      }
      pc = data;
      break;

    case 0x4C: /* EXIT2 */
      if (Exit2("EXIT2", mem_get(stack_adr, S_WORD))) return true;
      break;
    case 0x00: /* EXIT */
      if (Exit2("EXIT", 0)) return true;
      break;

    case 0x31: /* KEEPPR */
    {
      len = mem_get(stack_adr, S_LONG);
      UWord exit_code = mem_get(stack_adr + 4, S_WORD);
      if (func_trace_f) {
        printf("%-10s\n", "KEEPPR");
      }
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

    case 0xf7:  // BUS_ERR
    {
      short size = (short)mem_get(stack_adr, S_WORD);  // アクセスサイズ
      Long r_ptr = mem_get(stack_adr + 2, S_LONG);
      Long w_ptr = mem_get(stack_adr + 6, S_LONG);
      if (func_trace_f) {
        printf("%-10s size=%d P1.l=%08X P2.l=%08X\n", "BUS_ERR", size, r_ptr,
               w_ptr);
      }
      rd[0] = 1;  // BusErr( buf, data, srt );
    } break;

    default:
      if (func_trace_f) {
        printf("%-10s code=0xFF%02X\n", "????????", code);
      }
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

  char *buf_ptr = prog_ptr + buf;
  UByte max = buf_ptr[0];
  Long len = gets2(str, max);
  buf_ptr[1] = len;
  strcpy(&(buf_ptr[2]), str);
  return (len);
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

/*
   機能：
     DOSCALL CREATEを実行する
   パラメータ：
     Long  p         <in>    ファイルパス名文字列のポインタ
     short    atr       <in>    ファイル属性
   戻り値：
     Long  ファイルハンドル(>=0)
              エラーコード(<0)
 */
static Long Create(char *p, short atr) {
  Long i;

#ifndef WIN32
  char *name = p;
  int namelen = strlen(name);
  for (int i = 0; i < namelen; ++i) {
    if (name[i] == '\\') {
      name[i] = '/';
    }
    if (name[i] == ':') {
      p = &name[i + 1];
    }
  }
  // printf("Create(\"%s\", %d) = %s\n", name, atr, p);
#endif

  Long ret = find_free_file();
  if (ret < 0) return -4;  // オープンしているファイルが多すぎる

  /* ファイル名後ろの空白をつめる */
  int len = strlen(p);
  for (i = len - 1; i >= 0 && p[i] == ' '; i--) p[i] = '\0';

  /* ファイル名のチェック */
  if ((len = strlen(p)) > 88) return (-13); /* ファイル名の指定誤り */

  for (i = len - 1; i >= 0 && p[i] != '.'; i--)
    ;
  if (i >= 0) {
    /* 拡張子が存在する */
    if (strlen(&(p[i])) > 4) return (-13);
  }

#ifdef _WIN32
  HANDLE handle = CreateFile(p, GENERIC_WRITE | GENERIC_READ, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle == INVALID_HANDLE_VALUE) return -23;  // ディスクがいっぱい
  finfo[ret].host.handle = handle;
#else
  FILE *fp = fopen(p, "w+b");
  if (fp == NULL) return -23;  // ディスクがいっぱい
  finfo[ret].host.fp = fp;
#endif

  finfo[ret].is_opened = true;
  finfo[ret].mode = 2;
  finfo[ret].nest = nest_cnt;
  strcpy(finfo[ret].name, p);
  return (ret);
}

/*
 　機能：DOSCALL NEWFILEを実行する
 戻り値：ファイルハンドル(負ならエラーコード)
 */
static Long Newfile(char *p, short atr) {
  Long i;

  Long ret = find_free_file();
  if (ret < 0) return -4;  // オープンしているファイルが多すぎる

  /* ファイル名後ろの空白をつめる */
  Long len = strlen(p);
  for (i = len - 1; i >= 0 && p[i] == ' '; i--) p[i] = '\0';

  /* ファイル名のチェック */
  if ((len = strlen(p)) > 88) return (-13); /* ファイル名の指定誤り */

  for (i = len - 1; i >= 0 && p[i] != '.'; i--)
    ;
  if (i >= 0) {
    /* 拡張子が存在する */
    if (strlen(&(p[i])) > 4) return (-13);
  }
#ifdef _WIN32
  HANDLE handle = CreateFile(p, GENERIC_WRITE | GENERIC_READ, 0, NULL,
                             CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    DWORD e = GetLastError();
    if (e == ERROR_FILE_EXISTS) return DOSE_EXISTFILE;
    return -23;  // ディスクがいっぱい
  }
  finfo[ret].host.handle = handle;
#else
  FILE *fp = fopen(p, "rb");
  if (fp != NULL) {
    fclose(fp);
    return -80;  // 既に存在している
  }
  fp = fopen(p, "w+b");
  if (fp == NULL) return -23;  // ディスクがいっぱい
  finfo[ret].host.fp = fp;
#endif

  finfo[ret].is_opened = true;
  finfo[ret].mode = 2;
  finfo[ret].nest = nest_cnt;
  strcpy(finfo[ret].name, p);
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

#ifndef WIN32
  char *name = p;
  int namelen = strlen(name);
  for (int i = 0; i < namelen; ++i) {
    if (name[i] == '\\') {
      name[i] = '/';
    }
    if (name[i] == ':') {
      p = &name[i + 1];
    }
  }
  // printf("Open(\"%s\", %d) = %s\n", name, mode, p);
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
  strcpy(finfo[ret].name, p);
  return (ret);
}

/*
 　機能：DOSCALL CLOSEを実行する
 戻り値：エラーコード
 */
static Long Close(short hdl) {
  if (hdl <= HUMAN68K_SYSTEM_FILENO_MAX) return DOSE_SUCCESS;
  if (!finfo[hdl].is_opened) return -6;  // オープンされていない
  if (!CloseFile(&finfo[hdl])) return -14;  // 無効なパラメータでコールした

    /* タイムスタンプ変更 */
#ifdef _WIN32
  if (finfo[hdl].date != 0 || finfo[hdl].time != 0) {
    FILETIME ft0, ft1, ft2;
    int64_t datetime;

    HANDLE handle = CreateFileA(finfo[hdl].name, GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    GetFileTime(handle, &ft0, &ft1, &ft2);
    // 秒→100nsecに変換する。
    datetime = ((int64_t)finfo[hdl].date * 86400L + finfo[hdl].time) * 10000000;
    ft2.dwLowDateTime = (ULong)(datetime & 0xffffffff);
    ft2.dwHighDateTime = (ULong)(datetime >> 32);
    SetFileTime(handle, &ft0, &ft1, &ft2);
    CloseHandle(handle);
    finfo[hdl].date = 0;
    finfo[hdl].time = 0;
  }
#endif
  return (0);
}

/*
 　機能：DOSCALL FGETSを実行する
 戻り値：エラーコード
 */
static Long Fgets(Long adr, short hdl) {
  char buf[257];
  char *p;
  size_t len;

  if (!finfo[hdl].is_opened) return -6;  // オープンされていない
  if (finfo[hdl].mode == 1) return (-1);

  UByte max = (unsigned char)mem_get(adr, S_BYTE);
#ifdef _WIN32
  {
    BOOL b = FALSE;
    DWORD read_len;
    char c;
    int i;
    for (i = 0; i < max; i++) {
      b = ReadFile(finfo[hdl].host.handle, &c, 1, (LPDWORD)&read_len, NULL);
      if (c == '\r') {
        b = ReadFile(finfo[hdl].host.handle, &c, 1, (LPDWORD)&read_len, NULL);
        if (c == 'n') {
          buf[i] = '\0';
          break;
        } else {
          buf[i] = '\r';
        }
      }
    }
    if (b == FALSE) return -1;
  }
#else
  if (fgets(buf, max, finfo[hdl].host.fp) == NULL) return -1;
#endif
  len = strlen(buf);
  if (len < 2) return (-1);

  len -= 2;
  buf[len] = '\0';
  mem_set(adr + 1, len, S_BYTE);
  p = prog_ptr + adr + 2;
  strcpy(p, buf);

  return (len);
}

/*
 　機能：DOSCALL READを実行する
 戻り値：読み込んだバイト数(負ならエラーコード)
 */
static Long Read(short hdl, Long buf, Long len) {
  char *read_buf;
  Long read_len;

  if (!finfo[hdl].is_opened) return -6;  // オープンされていない
  if (finfo[hdl].mode == 1) return (-1); /* 無効なファンクションコール */

  if (len == 0) return (0);

  read_buf = prog_ptr + buf;
#ifdef _WIN32
  ReadFile(finfo[hdl].host.handle, read_buf, len, (LPDWORD)&read_len, NULL);
#elif defined(__APPLE__) || defined(__linux__) || defined(__EMSCRIPTEN__)
  read_len = Read_conv(hdl, read_buf, len);
#else
  read_len = fread(read_buf, 1, len, finfo[hdl].host.fp);
#endif

  return (read_len);
}

#if defined(__APPLE__) || defined(__linux__) || defined(__EMSCRIPTEN__)
static Long Read_conv(short hdl, void *buf, size_t size) {
  Long read_len;
  FILE *fp = finfo[hdl].host.fp;

  if (fp == NULL) return (-6);

  if (!isatty(fileno(fp))) {
    read_len = fread(buf, 1, size, fp);
  } else {
    int crlf_len;

    read_len = gets2(buf, size);
    crlf_len = size - read_len >= 2 ? 2 : size - read_len;
    memcpy(buf + read_len, "\r\n", crlf_len);
    read_len += crlf_len;
  }

  return read_len;
}
#endif

/*
 　機能：DOSCALL WRITEを実行する
 戻り値：書き込んだバイト数(負ならエラーコード)
 */
static Long Write(short hdl, Long buf, Long len) {
  char *write_buf;
  Long write_len = 0;

  if (!finfo[hdl].is_opened) return -6;  // オープンされていない

  if (len == 0) return (0);

  write_buf = prog_ptr + buf;
#ifdef _WIN32
  unsigned len2;
  WriteFile(finfo[hdl].host.handle, (LPCVOID)write_buf, len, &len2, NULL);
  write_len = len2;
  if (finfo[hdl].host.handle == GetStdHandle(STD_OUTPUT_HANDLE))
    FlushFileBuffers(finfo[hdl].host.handle);
#else
  write_len = Write_conv(hdl, write_buf, len);
#endif

  return (write_len);
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
  int err_save;
  unsigned int len;
  int hdl;
  int i;

  errno = 0;
  if (remove(p) != 0) {
    /* オープン中のファイルを調べる */
    err_save = errno;
    len = strlen(p);
    hdl = 0;
    for (i = 5; i < FILE_MAX; i++) {
      if (!finfo[i].is_opened || nest_cnt != finfo[i].nest) continue;
      if (len == strlen(finfo[i].name)) {
        if (memcmp(p, finfo[i].name, len) == 0) {
          hdl = i;
          break;
        }
      }
    }
    if (len > 0 && hdl >= HUMAN68K_USER_FILENO_MIN) {
      CloseFile(&finfo[hdl]);
      errno = 0;
      if (remove(p) != 0) {
        if (errno == ENOENT)
          return (-2); /* ファイルがない */
        else
          return (-13); /* ファイル名指定誤り */
      }
    } else {
      if (err_save == ENOENT)
        return (-2);
      else
        return (-13);
    }
  }
  return (0);
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
  char *old_ptr;
  char *new_ptr;

  old_ptr = prog_ptr + old;
  new_ptr = prog_ptr + new1;
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
  char *name_ptr;
  ULong ret;

  name_ptr = prog_ptr + adr;
  if (atr == -1) {
    /* 読み出し */
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
#ifdef _WIN32

  WIN32_FIND_DATA f_data;
  HANDLE handle;
  char *name_ptr;
  char *buf_ptr;
  name_ptr = prog_ptr + name;
  buf_ptr = prog_ptr + buf;

  /* 最初にマッチするファイルを探す。*/
  /* FindFirstFileEx()はWindowsNTにしかないのでボツ
handle = FindFirstFileEx
             (name_ptr,
              FindExInfoStandard,
              (LPVOID)&f_data,
              FindExSearchNameMatch,
              NULL,
              0);
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
  char *name_ptr;
  char *buf_ptr;
  name_ptr = prog_ptr + name;
  buf_ptr = prog_ptr + buf;

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
#ifdef _WIN32
  WIN32_FIND_DATA f_data;
  HANDLE handle;
  unsigned int i;
  char *buf_ptr;
  short atr;

  buf_ptr = prog_ptr + buf;
  atr = buf_ptr[0]; /* 検索すべきファイルの属性 */
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
    handle = *((HANDLE *)&buf_ptr[2]);
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
 　機能：DOSCALL GETENVを実行する
 戻り値：エラーコード
 */
static Long Getenv(Long name, Long env, Long buf) {
  Long ret;
  if (env != 0) return (-10);
  ret = Getenv_common(prog_ptr + name, prog_ptr + buf);
  return ret;
}

Long Getenv_common(const char *name_p, char *buf_p) {
  char *mem_ptr;
  /*
  WIN32の環境変数領域からrun68のエミュレーション領域に複製してある
  値を検索する仕様にする。
   */
  /*
  環境エリアの先頭(ENV_TOP)から順に環境変数名を検索する。
   */
  for (mem_ptr = prog_ptr + ENV_TOP + 4; *mem_ptr != 0; mem_ptr++) {
    char ename[256];
    int i;
    /* 環境変数名を取得する。*/
    for (i = 0; *mem_ptr != '\0' && *mem_ptr != '='; i++) {
      ename[i] = *(mem_ptr++);
    }
    ename[i] = '\0';
    if (_stricmp(name_p, ename) == 0) {
      /* 環境変数が見つかった。*/
      while (*mem_ptr == '=' || *mem_ptr == ' ') {
        mem_ptr++;
      }
      strcpy(buf_p, mem_ptr);
      return 0;
    }
    /* 変数名が一致しなかったら、変数の値をスキップする。*/
    while (*mem_ptr) mem_ptr++;
    /* '\0'の後にもう一つ'\0'が続く場合は、環境変数領域の終りである。*/
  }
  /* 変数が見つからなかったらNULLポインタを返す。*/
  (*buf_p) = 0;
  return -10;
}

/*
   機能：
     DOSCALL NAMESTSを実行する
   戻り値：
     エラーコード
 */
static Long Namests(Long name, Long buf) {
  char nbuf[256];
  char cud[67];
  char *name_ptr;
  char *buf_ptr;
  int wild = 0;
  int len;
  int i;

  name_ptr = prog_ptr + name;
  buf_ptr = prog_ptr + buf;
  memset(buf_ptr, 0x00, 88);
  if ((len = strlen(name_ptr)) > 88) return (-13); /* ファイル名の指定誤り */
  strcpy(nbuf, name_ptr);

  /* 拡張子をセット */
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
    if (Curdir(0, cud) != 0) return (13);
    strcpy(buf_ptr + 2, &(cud[2]));
    if (cud[strlen(cud) - 1] != '\\') strcat(buf_ptr + 2, "\\");
    nbuf[0] = cud[0];
    i = 1;
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
    GetCurrentDirectory(strlen(path), path);
    mem_set(buf + 1, path[0] - 'A', S_BYTE);
#endif
  } else {
    UByte drv = toupper(nbuf[0]) - 'A';
    if (drv >= 26) return (-13);
    mem_set(buf + 1, drv, S_BYTE);
  }

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
  char *name_ptr;
  char *buf_ptr;
  int ret = 0;
  int len;
  int i;

  name_ptr = prog_ptr + name;
  buf_ptr = prog_ptr + buf;
  memset(buf_ptr, 0x00, 91);
  if ((len = strlen(name_ptr)) > 88) return (-13); /* ファイル名の指定誤り */
  strcpy(nbuf, name_ptr);

  /* 拡張子をセット */
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
  if (i == 0) {
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
  char *p;
  Long mes;
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
      mes = mem_get(adr, S_LONG);
      p = prog_ptr + mes;
      printf("%s", p);
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
      } else {
        if (ini_info.pc98_key) c = cnv_key98(c);
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
      } else {
        if (ini_info.pc98_key) c = cnv_key98(c);
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
  char *buf_ptr;

  buf_ptr = prog_ptr + buf;

  if (mode < 256)
    get_fnckey(mode, buf_ptr);
  else
    put_fnckey(mode - 256, buf_ptr);
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
  Long drv;
  Long buf;
  char *drv_ptr;
  char *buf_ptr;

  switch (mode) {
    case 0:
      drv = mem_get(stack_adr, S_LONG);
      buf = mem_get(stack_adr + 4, S_LONG);
      drv_ptr = prog_ptr + drv;
      if (drv_ptr[1] != ':' || drv_ptr[2] != '\0') return (-14);
      drv = toupper(drv_ptr[0]) - 'A' + 1;
      if (drv < 1 || drv > 26) return (-14);
      if (Curdir((short)drv, prog_ptr + buf) != 0) return (-14);
      if (func_trace_f) {
        buf_ptr = prog_ptr + buf;
        printf(" drv=%s cudir=%s\n", drv_ptr, buf_ptr);
      }
      return (0x40);
    default:
      return (-14);
  }
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
  char *fcb_ptr;
  fcb_ptr = prog_ptr + FCB_WORK;
  switch (fhdl) {
    case 0:
    case 1:
    case 2:
      memcpy(fcb_ptr, fcb[fhdl], 0x60);
      return (FCB_WORK);
    default:
      fcb[3][14] = (unsigned char)fhdl;
      memcpy(fcb_ptr, fcb[3], 0x60);
      return (FCB_WORK);
  }
}

/*
 　機能：DOSCALL EXEC(mode=0,1)を実行する
 戻り値：エラーコード等
 */
static Long Exec01(Long nm, Long cmd, Long env, int md) {
  FILE *fp;
  char fname[89];
  char *name_ptr;
  int loadmode;
  Long prev_adr;
  Long end_adr;
  Long pc1;
  Long prog_size;
  Long prog_size2;

  loadmode = ((nm >> 24) & 0x03);
  nm &= 0xFFFFFF;
  name_ptr = prog_ptr + nm;
  if (strlen(name_ptr) > 88) return (-13); /* ファイル名指定誤り */

  strcpy(fname, name_ptr);
  if ((fp = prog_open(fname, false)) == NULL) return (-2);

  if (nest_cnt + 1 >= NEST_MAX) return (-8);

  // 最大メモリを確保する
  Long parent = psp[nest_cnt];
  ULong size = Malloc(MALLOC_FROM_LOWER, 0x00ffffff, parent) & 0x00ffffff;
  Long mem = Malloc(MALLOC_FROM_LOWER, size, parent);
  if (mem < 0) {
    fclose(fp);
    return -8;  // メモリが確保できない
  }

  prev_adr = mem_get(mem - 0x10, S_LONG);
  end_adr = mem_get(mem - 0x08, S_LONG);
  memset(prog_ptr + mem, 0, size);

  prog_size2 = ((loadmode << 24) | end_adr);
  pc1 = prog_read(fp, fname, mem - SIZEOF_MEMBLK + SIZEOF_PSP, &prog_size,
                  &prog_size2, false);
  if (pc1 < 0) {
    Mfree(mem);
    return (pc1);
  }

  nest_pc[nest_cnt] = pc;
  nest_sp[nest_cnt] = ra[7];
  ra[0] = mem - SIZEOF_MEMBLK;
  ra[1] = mem - SIZEOF_MEMBLK + SIZEOF_PSP + prog_size;
  ra[2] = cmd;
  ra[3] = (env == 0) ? mem_get(psp[nest_cnt] + PSP_ENV_PTR, S_LONG) : env;
  ra[4] = pc1;
  nest_cnt++;
  if (!make_psp(fname, prev_adr, end_adr, psp[nest_cnt - 1], prog_size2)) {
    nest_cnt--;
    Mfree(mem);
    return (-13);
  }

  if (md == 0) {
    pc = ra[4];
    return (rd[0]);
  } else {
    nest_cnt--;
    return (ra[4]);
  }
}

/*
 　機能：DOSCALL EXEC(mode=2)を実行する
 戻り値：エラーコード
 */
static Long Exec2(Long nm, Long cmd, Long env) {
  FILE *fp;
  char *name_ptr;
  char *cmd_ptr;
  char *p;
  int len;

  name_ptr = prog_ptr + nm;
  cmd_ptr = prog_ptr + cmd;
  p = name_ptr;
  while (*p != '\0' && *p != ' ') p++;
  if (*p != '\0') { /* コマンドラインあり */
    *p = '\0';
    p++;
    len = strlen(p);
    *cmd_ptr = len;
    strcpy(cmd_ptr + 1, p);
  }

  /* 環境変数PATHに従ってファイルを検索し、オープンする。*/
  fp = prog_open(name_ptr, true);
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
static Long Exec3(Long nm, Long adr1, Long adr2) {
  FILE *fp;
  char fname[89];
  char *name_ptr;
  int loadmode;
  Long ret;
  Long prog_size;
  Long prog_size2;

  loadmode = ((nm >> 24) & 0x03);
  nm &= 0xFFFFFF;
  adr1 &= 0xFFFFFF;
  adr2 &= 0xFFFFFF;
  name_ptr = prog_ptr + nm;
  if (strlen(name_ptr) > 88) return (-13); /* ファイル名指定誤り */

  strcpy(fname, name_ptr);
  if ((fp = prog_open(fname, false)) == NULL) return (-2);

  prog_size2 = ((loadmode << 24) | adr2);
  ret = prog_read(fp, fname, adr1, &prog_size, &prog_size2, false);
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
static Long gets2(char *str, int max) {
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

/*
        $fff7	_BUS_ERR	Check for bus errors

                Arg	SIZE.w		アクセスサイズ(1:バイト 2:ワード
   4:ロングワード) P1.l		読み込みポインタ P2.l		書き込みポインタ

                Ret	d0.l =  0	読み書き可能
                d0.l =  1	P2 に書き込んだ時にバスエラーが発生
                d0.l =  2	P1 から読み込んだ時にバスエラーが発生
                d0.l = -1	エラー(Argが異常)

                SIZE で指定されたサイズで P1
   で指定したアドレスから読み込み、そのデータ を P2
   で指定したアドレスに書き込んでバスエラーが発生するかどうか調べる. SIZE
   の値が異常な場合や SIZE = 2,4 で P1,P2 に奇数アドレスを指定した場 合はRetが
   -1 になる.

                move	SIZE,-(sp)
                pea	(P2)
                pea	(P1)
                DOS	_BUS_ERR
                lea	(10,sp),sp
*/

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
