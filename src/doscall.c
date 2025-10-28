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

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <direct.h>
#include <dos.h>
#include <io.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#ifdef USE_ICONV
#include <iconv.h>
#endif

#include "ansicolor-w32.h"
#include "dos_file.h"
#include "dos_memory.h"
#include "dos_misc.h"
#include "dostrace.h"
#include "host.h"
#include "human68k.h"
#include "iocscall.h"
#include "mem.h"
#include "operate.h"
#include "run68.h"

static Long Gets(Long);
static Long Kflush(short);
static Long Ioctrl(short, Long);
static Long Dup(short);
static Long Dup2(short org, short new);
static Long Dskfre(short, Long);
static Long Close(short);
static Long Fgets(Long, short);
static Long Write(short, Long, Long);
static Long Delete(char*);
static Long Rename(Long, Long);
static Long Files(Long, Long, short);
static Long Nfiles(Long);
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
static Long Write_conv(short hdl, void* buf, size_t size) {
  Long write_len;
  FILE* fp = finfo[hdl].host.fp;

  if (fp == NULL) return -6;

#ifdef USE_ICONV
  if (isatty(fileno(fp))) {
    static char prev_char = 0;
    iconv_t icd = iconv_open("UTF-8", "Shift_JIS");

    write_len = 0;

    while (size > 0) {
      char sjis_buf[2048];
      char utf8_buf[4096];
      size_t sjis_buf_size;
      char* sjis_buf_p;
      size_t sjis_bytes;
      char* utf8_buf_p = utf8_buf;
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
    return write_len;
  }
#endif

  return fwrite(buf, 1, size, fp);
}
#endif

#ifndef _WIN32
void _flushall() {}

char _getch() {
  printf("_getch()\n");
  return getchar();
}

char _getche() {
  printf("_getche()\n");
  return getchar();
}

static void dos_getdrive(Long* drv) {
  *drv = 1;  // 1 = A:
}

static void dos_setdrive(Long drv, Long* dmy) {}

int _kbhit() { return 1; }

int kbhit() { return 1; }

static char ungetch(char c) {
  printf("ungetch()\n");
  return 0;
}
#endif

static int fgetcFromOnmemory(FILEINFO* finfop) {
  Long pos = finfop->onmemory.position;
  if (pos >= finfop->onmemory.length) return DOSE_ILGFNC;

  finfop->onmemory.position += 1;
  return finfop->onmemory.buffer[pos];
}

// DOS _FGETC (0xff1b)
static Long DosFgetc(ULong param) {
  UWord fileno = ReadParamUWord(&param);
  if (fileno >= FILE_MAX) return DOSE_MFILE;

  FILEINFO* finfop = &finfo[fileno];
  if (!finfop->is_opened) return DOSE_BADF;

  if (finfop->onmemory.buffer) return (Long)fgetcFromOnmemory(finfop);

#ifdef _WIN32
  char c = 0;
  DWORD read_len = 0;
  if (GetFileType(finfop->host.handle) == FILE_TYPE_CHAR) {
    /* 標準入力のハンドルがキャラクタタイプだったら、ReadConsoleを試してみる。*/
    while (true) {
      INPUT_RECORD ir;
      BOOL b =
          ReadConsoleInput(finfop->host.handle, &ir, 1, (LPDWORD)&read_len);
      if (b == FALSE) {
        /* コンソールではなかった。*/
        if (!ReadFile(finfop->host.handle, &c, 1, (LPDWORD)&read_len, NULL))
          c = 0;
        break;
      }
      if (read_len == 1 && ir.EventType == KEY_EVENT &&
          ir.Event.KeyEvent.bKeyDown) {
        c = ir.Event.KeyEvent.uChar.AsciiChar;
        if (0x01 <= c && c <= 0xff) break;
      }
    }
  } else {
    if (!ReadFile(finfop->host.handle, &c, 1, (LPDWORD)&read_len, NULL)) c = 0;
  }
  return (read_len == 0) ? DOSE_ILGFNC : c;
#else
  int ch = fgetc(finfop->host.fp);
  return (0 <= ch && ch <= 0xff) ? ch : DOSE_ILGFNC;
#endif
}

// ファイルを閉じてFILEINFOを未使用状態に戻す
static bool CloseFile(FILEINFO* finfop) {
  finfop->is_opened = false;
  FreeOnmemoryFile(finfop);
  return HOST_CLOSE_FILE(finfop);
}

// 現在のプロセスが開いたファイルを全て閉じる
void close_all_files(void) {
  int i;

  for (i = HUMAN68K_USER_FILENO_MIN; i < FILE_MAX; i++) {
    FILEINFO* finfop = &finfo[i];
    if (finfop->is_opened && finfop->nest == nest_cnt) {
      CloseFile(finfop);
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
  char* data_ptr = 0;
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
      FILEINFO* finfop = &finfo[1];
      if (GetConsoleMode(finfop->host.handle, &st) != 0) {
        // 非リダイレクト
        WriteW32(1, finfop->host.handle, c, 1);
      } else {
        Long nwritten = 0;
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
        FILEINFO* finfop = &finfo[0];
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
        FILEINFO* finfop = &finfo[1];
        if (GetConsoleMode(finfop->host.handle, &st) != 0) {
          WriteW32(1, finfop->host.handle, data_ptr, len);
        } else {
          Long nwritten = 0;
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
    case 0x1b:  // FGETC
      rd[0] = DosFgetc(stack_adr);
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
      FILEINFO* finfop = &finfo[fhdl];
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
    case 0x27:  // GETTIM2
      rd[0] = DosGettim2();
      break;
    case 0x28:  // SETTIM2
      rd[0] = DosSettim2(stack_adr);
      break;
    case 0x29: /* NAMESTS */
      data = mem_get(stack_adr, S_LONG);
      buf = mem_get(stack_adr + 4, S_LONG);
      rd[0] = Namests(data, buf);
      break;
    case 0x2a:  // GETDATE
      rd[0] = DosGetdate();
      break;
    case 0x2b:  // SETDATE
      rd[0] = DosSetdate(stack_adr);
      break;
    case 0x2c:  // GETTIME
      rd[0] = DosGettime();
      break;
    case 0x2d:  // SETTIME
      rd[0] = DosSettime(stack_adr);
      break;
    case 0x30:  // VERNUM
      rd[0] = DosVernum();
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
    case 0x39:  // MKDIR
      rd[0] = DosMkdir(stack_adr);
      break;
    case 0x3a:  // RMDIR
      rd[0] = DosRmdir(stack_adr);
      break;
    case 0x3b:  // CHDIR
      rd[0] = DosChdir(stack_adr);
      break;
    case 0x3c:  // CREATE
      rd[0] = DosCreate(stack_adr);
      break;
    case 0x3d:  // OPEN
      rd[0] = DosOpen(stack_adr);
      break;
    case 0x3E: /* CLOSE */
      srt = (short)mem_get(stack_adr, S_WORD);
      rd[0] = Close(srt);
      break;
    case 0x3f:  // READ
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
    case 0x42:  // SEEK
      rd[0] = DosSeek(stack_adr);
      break;
    case 0x43:  // CHMOD
      rd[0] = DosChmod(stack_adr);
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
    case 0x47:  // CURDIR
      rd[0] = DosCurdir(stack_adr);
      break;
    case 0x48:  // MALLOC
      rd[0] = DosMalloc(stack_adr);
      break;
    case 0x49:  // MFREE
      rd[0] = DosMfree(stack_adr);
      break;
    case 0x4a:  // SETBLOCK
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

  Long ret = FindFreeFileNo();
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
  ULong SectorsPerCluster = 0, BytesPerSector = 0, NumberOfFreeClusters = 0,
        TotalNumberOfClusters = 0;

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
static char* to_slash(size_t size, char* buf, const char* path) {
  if (strlen(path) >= size) return NULL;

  char* p = strncpy(buf, path, size);
  char* root = (p[0] && p[1] == ':') ? p + strlen("A:") : p;
  do {
    if (*p == '\\') *p = '/';
  } while (*p++);

  return root;
}
#endif

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

static Long fgetsFromOnmemory(FILEINFO* finfop, ULong adr) {
  UByte rest = ReadUByteSuper(adr + 0);
  ULong write = adr + 2;
  Long len = 0;

  while (rest > 0) {
    int c = fgetcFromOnmemory(finfop);
    if (c < 0) {
      if (len == 0) len = DOSE_ILGFNC;  // 1バイトも入力できなければエラー
      break;
    }
    if (c == '\n') break;     // LFなら終了
    if (c == '\r') continue;  // CRは無視する

    WriteUByteSuper(write++, (UByte)c);
    len += 1;
    rest -= 1;
    if (c == '\x1a') break;  // EOFは書き込んだ上で終了
  }
  WriteUByteSuper(write, 0);

  WriteUByteSuper(adr + 1, (UByte)len);
  return len;
}

/*
 　機能：DOSCALL FGETSを実行する
 戻り値：エラーコード
 */
static Long Fgets(Long adr, short hdl) {
  char buf[257] = {0};
  size_t len;

  FILEINFO* finfop = &finfo[hdl];

  if (!finfop->is_opened) return -6;  // オープンされていない
  if (finfop->mode == 1) return (-1);

  if (finfop->onmemory.buffer) return fgetsFromOnmemory(finfop, adr);

  UByte max = ReadUByteSuper(adr);
#ifdef _WIN32
  {
    int i = 0;
    while (i < max) {
      DWORD read_len = 0;
      char c = 0;

      if (ReadFile(finfop->host.handle, &c, 1, (LPDWORD)&read_len, NULL) ==
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
    if (fgets(buf, max, finfop->host.fp) == NULL) return -1;
    char* s = buf;
    char* d = buf;
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
  unsigned len2 = 0;
  WriteFile(finfo[hdl].host.handle, mem.bufptr, mem.length, &len2, NULL);
  write_len = len2;
  if (finfo[hdl].host.handle == GetStdHandle(STD_OUTPUT_HANDLE))
    FlushFileBuffers(finfo[hdl].host.handle);
#else
  write_len = Write_conv(hdl, mem.bufptr, mem.length);
#endif

  return write_len;
}

/*
 　機能：DOSCALL DELETEを実行する
 戻り値：ファイルハンドル(負ならエラーコード)
 */
static Long Delete(char* p) {
  if (remove(p) != 0) return (errno == ENOENT) ? DOSE_NOENT : DOSE_ILGFNAME;
  return DOSE_SUCCESS;
}

/*
 　機能：DOSCALL RENAMEを実行する
 戻り値：エラーコード
 */
static Long Rename(Long old, Long new1) {
  char* old_ptr = GetStringSuper(old);
  char* new_ptr = GetStringSuper(new1);

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
  char* name_ptr = GetStringSuper(name);

  ULong bufSize = 53;
  bool exMode = (buf & 0x80000000) ? true : false;
  if (exMode) {
    // buf &= ~0x80000000;
    // bufSize = 141;
    return DOSE_ILGFNC;
  }

  Span mem = GetWritableMemorySuper(buf, bufSize);
  if (!mem.bufptr) throwBusErrorOnWrite(buf + mem.length);
  char* buf_ptr = mem.bufptr;

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
  buf_ptr[0] = atr;                 /* ファイルの属性 */
  buf_ptr[1] = 0;                   /* ドライブ番号(not used) */
  *((HANDLE*)&buf_ptr[2]) = handle; /* サーチハンドル */
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
    ULong size = 0;
    bool success = false;
    struct stat st;
    if (stat(name_ptr, &st) == 0) {
      if (S_ISREG(st.st_mode)) {
        size = st.st_size;
        success = true;
      } else if (S_ISDIR(st.st_mode)) {
        success = true;
      }
    }
    if (success) {
      /* 予約領域をセット */
      buf_ptr[0] = atr; /* ファイルの属性 */
      buf_ptr[1] = 0;   /* ドライブ番号(not used) */
      //			*((HANDLE*)&buf_ptr[2]) = handle; /*
      // サーチハンドル */
      // DATEとTIMEをセット: 未実装

      // FILELENをセット
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

  char* path = name_ptr;
  DIR* dir;
  struct dirent* dent;

  dir = opendir(path);
  printf("opendir(%s)=%p\n", path, dir);
  if (dir) {
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
  WIN32_FIND_DATA f_data = {0};
  char* buf_ptr = mem.bufptr;
  short atr = buf_ptr[0]; /* 検索すべきファイルの属性 */

  {
    /* todo:buf_ptrの指す領域から必要な情報を取り出して、f_dataにコピーする。*/
    /* 2秒→100nsに変換する。*/
    unsigned short s1 = *((unsigned short*)&buf_ptr[24]);
    unsigned short s2 = *((unsigned short*)&buf_ptr[22]);
    SYSTEMTIME st = {
        .wYear = ((s1 & 0xfe00) >> 9) + 1980,
        .wMonth = (s1 & 0x01e0) >> 5,
        .wDay = (s1 & 0x1f),
        .wHour = (s2 & 0xf800) >> 11,
        .wMinute = (s2 & 0x07e0) >> 5,
        .wSecond = (s2 & 0x001f),
        .wMilliseconds = 0,
    };
    SystemTimeToFileTime(&st, &f_data.ftLastWriteTime);

    f_data.nFileSizeHigh = 0;
    f_data.nFileSizeLow = *((ULong*)&buf_ptr[29]);
    /* ファイルのハンドルをバッファから取得する。*/
    HANDLE handle = *((HANDLE*)&buf_ptr[2]);
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
   機能：
     DOSCALL NAMESTSを実行する
   戻り値：
     エラーコード
 */
static Long Namests(Long name, Long buf) {
  char nbuf[256];
  int wild = 0;
  const char* name_ptr = GetStringSuper(name);

  Span mem = GetWritableMemorySuper(buf, SIZEOF_NAMESTS);
  if (!mem.bufptr) throwBusErrorOnWrite(buf + mem.length);
  char* buf_ptr = mem.bufptr;
  memset(buf_ptr, 0x00, SIZEOF_NAMESTS);

  int len = strlen(name_ptr);
  if (len > 88) return (-13); /* ファイル名の指定誤り */
  strcpy(nbuf, name_ptr);

  /* 拡張子をセット */
  int i;
  for (i = len - 1; i >= 0 && nbuf[i] != '.'; i--) {
    if (nbuf[i] == '*' || nbuf[i] == '?') wild = 1;
  }
  if (i < 0) {
    /* 拡張子なし */
    i = len;
    buf_ptr[75] = '\0';
  } else {
    if (strlen(&(nbuf[i])) > 4) return (-13);
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
    if (HOST_CURDIR(0, cud) != 0) return DOSE_ILGFNAME;
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
    GetCurrentDirectory(sizeof(path), path);
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
  char* name_ptr = GetStringSuper(name);

  Span mem = GetWritableMemorySuper(buf, SIZEOF_NAMECK);
  if (!mem.bufptr) throwBusErrorOnWrite(buf + mem.length);
  char* buf_ptr = mem.bufptr;
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
    const char* drv_ptr = GetStringSuper(drv);

    if (drv_ptr[1] != ':' || drv_ptr[2] != '\0') return DOSE_ILGPARM;
    short d = toupper(drv_ptr[0]) - 'A' + 1;
    if (d < 1 || d > 26) return DOSE_ILGPARM;

    char dir[HUMAN68K_DIR_MAX + 1];
    if (HOST_CURDIR(d, dir) != 0) return DOSE_ILGPARM;
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
  static unsigned char fcb[4][SIZEOF_FCB] = {
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

  ULong adr = GetFcbAddress((UWord)fhdl);

  Span mem = GetWritableMemorySuper(adr, SIZEOF_FCB);
  if (!mem.bufptr) throwBusErrorOnWrite(adr + mem.length);

  switch (fhdl) {
    case 0:
    case 1:
    case 2:
      memcpy(mem.bufptr, fcb[fhdl], SIZEOF_FCB);
      return adr;
    default:
      fcb[3][14] = (unsigned char)fhdl;
      memcpy(mem.bufptr, fcb[3], SIZEOF_FCB);
      return adr;
  }
}

// DOS _EXECの引数(実行ファイル名)からファイルタイプを抽出する。
//   実行ファイル名の最上位バイトをクリアする。
static bool getExecType(ULong* refFile, ExecType* outType) {
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
  FILE* fp;
  char fname[89];

  ExecType execType;
  if (!getExecType(&nm, &execType)) return DOSE_ILGPARM;

  char* name_ptr = GetStringSuper(nm);
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
  char* name_ptr = GetStringSuper(nm);
  char* cmd_ptr = GetStringSuper(cmd);

  char* p = name_ptr;
  while (*p != '\0' && *p != ' ') p++;
  if (*p != '\0') { /* コマンドラインあり */
    *p = '\0';
    p++;
    *cmd_ptr = strlen(p);
    strcpy(cmd_ptr + 1, p);
  }

  /* 環境変数pathに従ってファイルを検索し、オープンする。*/
  FILE* fp = prog_open(name_ptr, env, print);
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

  char* name_ptr = GetStringSuper(nm);
  if (strlen(name_ptr) > 88) return (-13); /* ファイル名指定誤り */

  strcpy(fname, name_ptr);
  FILE* fp = prog_open(fname, (ULong)-1, NULL);
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
Long gets2(char* str, int max) {
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
  DWORD dmy = 0;
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
