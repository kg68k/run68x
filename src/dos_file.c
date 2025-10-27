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

#include "dos_file.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "human68k.h"
#include "mem.h"
#include "run68.h"

// 開いている(オープン中でない)ファイル番号を探す
Long FindFreeFileNo(void) {
  int i;

  for (i = HUMAN68K_USER_FILENO_MIN; i < FILE_MAX; i++) {
    if (!finfo[i].is_opened) {
      return (Long)i;
    }
  }
  return (Long)-1;
}

static size_t strlenWithoutTrailingSpaces(const char* s) {
  size_t len = strlen(s);
  while (len > 0 && s[len - 1] == ' ') len--;
  return len;
}

static bool makeHuman68kPathName(ULong file, Human68kPathName* hpn) {
  char localBuf[128];
  const char* filename = GetStringSuper(file);

  size_t len = strlenWithoutTrailingSpaces(filename);
  if (len == 0) return false;

  char* buf = (len < sizeof(localBuf)) ? localBuf : malloc(len + 1);
  if (buf) {
    memcpy(buf, filename, len);
    buf[len] = '\0';
    bool result = HOST_CANONICAL_PATHNAME(buf, hpn);
    if (buf != localBuf) free(buf);
    return result;
  }

  return false;
}

// 新しいファイルを作成する。
Long CreateNewfile(ULong file, UWord atr, bool newfile) {
  Human68kPathName hpn;
  if (!makeHuman68kPathName(file, &hpn)) return DOSE_ILGFNAME;

  int fileno = FindFreeFileNo();
  if (fileno < 0) return DOSE_MFILE;  // オープンしているファイルが多すぎる。

  char fullpath[HUMAN68K_PATH_MAX + 1];
  strcat(strcpy(fullpath, hpn.path), hpn.name);

  HostFileInfoMember hostfile;
  Long err = HOST_CREATE_NEWFILE(fullpath, &hostfile, newfile);
  if (err != 0) return err;

  SetFinfo(fileno, hostfile, OPENMODE_READ_WRITE, nest_cnt);
  return fileno;
}

// ファイルを開く。
Long OpenExistingFile(ULong file, UWord mode) {
  FileOpenMode rwMode = mode & 0x000f;
  if (rwMode > OPENMODE_READ_WRITE) return DOSE_ILGARG;

  // シェアリングモードは未対応(常に許可する)
  if ((mode & 0x00f0) >= 0x0050) return DOSE_ILGARG;

  Human68kPathName hpn;
  if (!makeHuman68kPathName(file, &hpn)) return DOSE_ILGFNAME;

  int fileno = FindFreeFileNo();
  if (fileno < 0) return DOSE_MFILE;

  char fullpath[HUMAN68K_PATH_MAX + 1];
  strcat(strcpy(fullpath, hpn.path), hpn.name);

  HostFileInfoMember hostfile;
  Long err = HOST_OPEN_FILE(fullpath, &hostfile, rwMode);
  if (err != 0) return err;

  FILEINFO* finfop = SetFinfo(fileno, hostfile, rwMode, nest_cnt);
  ReadOnmemoryFile(finfop, rwMode);
  return fileno;
}

static Long readFile(FILEINFO* finfop, char* buffer, ULong length) {
  if (!finfop->onmemory.buffer)
    return HOST_READ_FILE_OR_TTY(finfop, buffer, length);

  ULong rest = (ULong)(finfop->onmemory.length - finfop->onmemory.position);
  ULong len = (rest < length) ? rest : length;

  memcpy(buffer, finfop->onmemory.buffer + finfop->onmemory.position, len);
  finfop->onmemory.position += len;

  return len;
}

// DOS _READ
Long Read(UWord fileno, ULong buffer, ULong length) {
  // Human68k v3.02ではファイルのエラー検査より先にバイト数が0か調べている
  if (length == 0) return 0;

  FILEINFO* finfop = &finfo[fileno];
  if (!finfop->is_opened) return DOSE_BADF;

  // 書き込みモードで開いたファイルでも読み込むことができるので
  // オープンモードは確認しない

  Span mem;
  if (!GetWritableMemoryRangeSuper(buffer, length, &mem))
    throwBusErrorOnWrite(buffer);  // バッファアドレスが不正

  Long result = readFile(finfop, mem.bufptr, mem.length);
  if (result <= 0) return result;
  if (length == mem.length) return result;  // バッファが全域有効なら完了

  if ((ULong)result < mem.length) {
    // 有効なバッファより少ないバイト数だけ読み込めたなら完了
    // 例えば buffer=0x00bffffe, length=4 で result==1 の場合
    return result;
  }

  // 有効なバッファちょうどのバイト数だけ読み込めた場合は、残りのバイト数を
  // 後続の不正なアドレスに読み込む動作を偽装する。

  // 試しに追加で1バイト読み込んでみる
  char dummy;
  Long result2 = readFile(finfop, &dummy, 1);
  if (result2 < 0) return result2;

  // ファイル末尾に達していたら、最初の読み込みでちょうど終わっていた
  if (result2 == 0) return result;

  // 追加で読めてしまったらバスエラー発生
  throwBusErrorOnWrite(buffer + mem.length);
}

static Long seekOnmemoryFile(FILEINFO* finfop, Long offset, FileSeekMode mode) {
  Long base = (mode == SEEKMODE_SET)   ? 0
              : (mode == SEEKMODE_CUR) ? finfop->onmemory.position
                                       : finfop->onmemory.length;
  Long pos = base + offset;

  if (pos < 0 || pos > finfop->onmemory.length) return DOSE_CANTSEEK;

  finfop->onmemory.position = pos;
  return pos;
}

// DOS _SEEK (0xff42)
Long Seek(UWord fileno, Long offset, UWord mode) {
  if (fileno >= FILE_MAX) return DOSE_MFILE;

  FILEINFO* finfop = &finfo[fileno];
  if (!finfop->is_opened) return DOSE_BADF;

  if (mode > SEEKMODE_END) return DOSE_ILGPARM;

  return finfop->onmemory.buffer ? seekOnmemoryFile(finfop, offset, mode)
                                 : HOST_SEEK_FILE(finfop, offset, mode);
}

// DOS _CHMOD (0xff43)
Long DosChmod(ULong param) {
  ULong file = ReadParamULong(&param);
  UWord atr = ReadParamUWord(&param);

  Human68kPathName hpn;
  if (!makeHuman68kPathName(file, &hpn)) return DOSE_ILGFNAME;

  // パスデリミタで終わっていればエラー
  if (hpn.name[0] == '\0') return DOSE_ILGFNAME;

  // ドライブ名だけ(D:)ならエラー
  // とりあえずの対処なのでいずれ見直す。
  const char* filename = GetStringSuper(file);
  if (isalpha(filename[0]) && filename[1] == ':' && filename[2] == '\0')
    return DOSE_ILGFNAME;

  // TODO: ワイルドカードを使用している場合は DOSE_ILGFNAME エラーにする。
  // (現状はwin32では-2が返る)

  char fullpath[HUMAN68K_PATH_MAX + 1];
  strcat(strcpy(fullpath, hpn.path), hpn.name);

  if (atr == (UWord)-1) return HOST_GET_FILE_ATTRIBUTE(fullpath);
  return HOST_SET_FILE_ATTRIBUTE(fullpath, atr);
}

// 最後のパスデリミタ(\ : /)の次のアドレスを求める
//   パスデリミタがなければ文字列先頭を返す
static char* get_filename(char* path) {
  char* filename = path;
  char* p = path;
  char c;

  while ((c = *p++) != '\0') {
    if (c == '\\' || c == ':' || c == '/') {
      filename = p;
      continue;
    }
    if (is_mb_lead(c)) {
      if (*p++ == '\0') break;
    }
  }
  return filename;
}

// 文字列中の文字を置換する
static void replace_char(char* s, char from, char to) {
  for (; *s; ++s) {
    if (*s == from) *s = to;
  }
}

// DOS _MAKETMP
Long Maketmp(ULong path, UWord atr) {
  char* const path_buf = GetStringSuper(path);

  char* const filename = get_filename(path_buf);
  const size_t len = strlen(filename);
  if (len == 0) return DOSE_ILGFNAME;

  replace_char(filename, '?', '0');  // ファイル名中の'?'を'0'に置き換える

  for (;;) {
    Long fileno = CreateNewfile(path, atr, true);
    if (fileno != DOSE_EXISTFILE) {
      // ファイルを作成できれば終了
      // 同名ファイルが存在する以外のエラーでも終了
      return fileno;
    }

    // 同名ファイルが存在するエラーの場合は、ファイル名中の数字に1を加算する
    bool done = false;
    for (char* t = filename + len - 1; filename <= t; --t) {
      if (!isdigit(*t)) continue;

      if (*t == '9') {
        *t = '0';  // '9'は'0'に戻して上位桁に繰り上げる
        continue;
      }
      *t += 1;  // '0' -> '1', ... '8' -> '9'
      done = true;
      break;  // 加算が完了したのでループを抜ける
    }
    if (!done) {
      // 数字がないか、最上位桁からの繰り上げができなかった場合はエラー
      return DOSE_EXISTFILE;
    }

    // 加算できたらファイル作成を再試行する
  }
}

static OnmemoryFileData defaultOnmemoryFileData(void) {
  return (OnmemoryFileData){NULL, 0, 0};
}

// finfoを初期化する。
void ClearFinfo(int fileno) {
  FILEINFO* f = &finfo[fileno];

  f->host = (HostFileInfoMember){0};
  f->is_opened = false;
  f->mode = OPENMODE_READ;
  f->nest = 0;
  f->onmemory = defaultOnmemoryFileData();
}

// オープンしたファイルの情報をfinfoに書き込む。
FILEINFO* SetFinfo(int fileno, HostFileInfoMember hostfile, FileOpenMode mode,
                   unsigned int nest) {
  FILEINFO* f = &finfo[fileno];

  f->host = hostfile;
  f->is_opened = true;
  f->mode = mode;
  f->nest = nest_cnt;
  f->onmemory = defaultOnmemoryFileData();

  return f;
}

void FreeOnmemoryFile(FILEINFO* finfop) {
  if (!finfop->onmemory.buffer) return;

  free(finfop->onmemory.buffer);
  finfop->onmemory.buffer = NULL;
}

void ReadOnmemoryFile(FILEINFO* finfop, FileOpenMode openMode) {
  if (openMode != OPENMODE_READ || !settings.readFileUtf8) return;

  Long fileSize = HOST_SEEK_FILE(finfop, 0, SEEKMODE_END);
  HOST_SEEK_FILE(finfop, 0, SEEKMODE_SET);
  if (fileSize < 0) return;

  char* u8buf = malloc(fileSize);
  if (!u8buf) return;

  Long readSize = (Long)HOST_READ_FILE_OR_TTY(finfop, u8buf, fileSize);
  HOST_SEEK_FILE(finfop, 0, SEEKMODE_SET);
  if (readSize != fileSize) {
    free(u8buf);
    return;
  }

  size_t sjSize;
  char* sjbuf = HOST_UTF8_TO_SJIS(u8buf, readSize, &sjSize);
  free(u8buf);
  if (!sjbuf) return;

  finfop->onmemory.buffer = sjbuf;
  finfop->onmemory.length = sjSize;
  finfop->onmemory.position = 0;
}
