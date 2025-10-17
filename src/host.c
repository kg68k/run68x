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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef USE_ICONV
#include <iconv.h>
#endif

#include "host.h"
#include "human68k.h"
#include "run68.h"

#define ROOT_SLASH_LEN 1  // "/"
#define DEFAULT_DRV_CLN "A:"

#ifdef HOST_TO_LOCALTIME_GENERIC
struct tm* ToLocaltime_generic(const time_t* timer, struct tm* result) {
  return localtime_r(timer, result);
}
#endif

#ifdef HOST_UTF8_TO_SJIS_GENERIC_ICONV
// UTF-8からShift_JISへの変換
char* Utf8ToSjis2_generic_iconv(char* inbuf, size_t inbytes,
                                size_t* outBufSize) {
  *outBufSize = 0;

  size_t bufsize = inbytes;
  char* sjbuf = malloc(bufsize);
  if (!sjbuf) return NULL;

  iconv_t icd = iconv_open("CP932", "UTF-8");
  char* outbuf = sjbuf;
  size_t outbytes = bufsize;
  size_t len = iconv(icd, &inbuf, &inbytes, &outbuf, &outbytes);
  iconv_close(icd);
  if (len == (size_t)-1) {
    free(sjbuf);
    return NULL;
  }

  size_t consumedSize = bufsize - outbytes;
  char* sjbuf2 = realloc(sjbuf, consumedSize);
  if (!sjbuf2) sjbuf2 = sjbuf;

  *outBufSize = consumedSize;
  return sjbuf2;
}
#endif

#ifdef HOST_CONVERT_TO_SJIS_GENERIC_ICONV
// ホスト文字列(UTF-8)からShift_JIS文字列への変換
bool Utf8ToSjis_generic_iconv(char* inbuf, char* outbuf, size_t outbuf_size) {
  iconv_t icd = iconv_open("CP932", "UTF-8");
  size_t inbytes = strlen(inbuf);
  size_t outbytes = outbuf_size - 1;
  size_t len = iconv(icd, &inbuf, &inbytes, &outbuf, &outbytes);
  iconv_close(icd);
  *outbuf = '\0';
  return len != (size_t)-1;
}
#endif

#ifdef HOST_CONVERT_FROM_SJIS_GENERIC_ICONV
// Shift_JIS文字列からホスト文字列(UTF-8)への変換
bool SjisToUtf8_generic_iconv(char* inbuf, char* outbuf, size_t outbuf_size) {
  iconv_t icd = iconv_open("UTF-8", "CP932");
  size_t inbytes = strlen(inbuf);
  size_t outbytes = outbuf_size - 1;
  size_t len = iconv(icd, &inbuf, &inbytes, &outbuf, &outbytes);
  iconv_close(icd);
  *outbuf = '\0';
  return len != (size_t)-1;
}
#endif

#if defined(HOST_CONVERT_TO_SJIS_GENERIC) || \
    defined(HOST_CONVERT_FROM_SJIS_GENERIC)
// Shift_JIS文字列からShift_JIS文字列への無変換コピー
bool SjisToSjis_generic(char* inbuf, char* outbuf, size_t outbuf_size) {
  size_t len = strlen(inbuf);
  if (len >= outbuf_size) return false;
  strcpy(outbuf, inbuf);
  return true;
}
#endif

// Shift_JIS文字列中のスラッシュをバックスラッシュに書き換える
static void to_backslash(char* s) {
  for (; *s; s += 1) {
    if (*s == '/') {
      *s = '\\';
    }
  }
}

// Shift_JIS文字列中のバックスラッシュをスラッシュに書き換える
static void to_slash(char* s) {
  while (*s) {
    if (*s == '\\') *s = '/';
    s += (is_mb_lead(s[0]) && s[1]) ? 2 : 1;
  }
}

static size_t getDriveNameLen(const char* path) {
  return (path[0] && path[1] == ':' && !is_mb_lead(path[0])) ? 2 : 0;
}

#ifdef HOST_CANONICAL_PATHNAME_GENERIC
#include <unistd.h>

static void parentPath(char* buf) {
  size_t len = strlen(buf);
  if (len <= 1) return;

  buf[len - 1] = '\0';
  char* s = strrchr(buf, '/');
  if (s != NULL) s[1] = '\0';
}

static bool isPathDelimiter(char c) { return (c == '/') || (c == '\\'); }

static const char* findPathDelimiter(const char* s) {
  while (*s) {
    if (isPathDelimiter(*s)) return s;
    s += (is_mb_lead(s[0]) && s[1]) ? 2 : 1;
  }
  return NULL;
}

static const char* skipPathDelimiter(const char* s) {
  while (isPathDelimiter(*s)) s += 1;
  return s;
}

static char* absolutePath(const char* path, size_t bufsize, char* buf) {
  path = path + getDriveNameLen(path);

  if (isPathDelimiter(*path)) {
    strcpy(buf, "/");
    path = skipPathDelimiter(path);
  } else {
    if (getcwd(buf, bufsize - 1) == NULL) return NULL;
    if (strcmp(buf, "/") != 0) strcat(buf, "/");
  }

  while (*path) {
    if (strcmp(path, ".") == 0) {
      break;
    }
    if (strcmp(path, "..") == 0) {
      parentPath(buf);
      break;
    }

    if (path[0] == '.' && isPathDelimiter(path[1])) {
      path += 2;
      continue;
    }
    if (memcmp(path, "..", 2) == 0 && isPathDelimiter(path[2])) {
      path += 3;
      parentPath(buf);
      continue;
    }

    size_t buf_len = strlen(buf);
    char* write_ptr = buf + buf_len;
    const char* sep = findPathDelimiter(path);
    if (sep) {
      // パスデリミタあり
      size_t len = (size_t)(sep - path);
      if ((buf_len + len + 1) >= bufsize) return NULL;
      memcpy(write_ptr, path, len);
      strcpy(write_ptr + len, "/");
      path = skipPathDelimiter(path + len);
    } else {
      // パスデリミタなし
      size_t len = strlen(path);
      if ((buf_len + len) >= bufsize) return NULL;
      strcpy(write_ptr, path);
      break;
    }
  }

  return buf;
}

static bool canonical_pathname(const char* fullpath, Human68kPathName* hpn) {
  char* lastSlash = strrchr(fullpath, '/');
  if (lastSlash == NULL) return false;

  char* name = lastSlash + 1;
  size_t pathLen = name - fullpath;
  if (pathLen > HUMAN68K_DIR_MAX) return false;

  size_t nameLen = strlen(name);  // 拡張子を含む長さなので後で差し引く
  char* ext = strrchr(name, '.');
  if (ext == NULL) ext = name + nameLen;

  size_t extLen = strlen(ext);
  nameLen -= extLen;
  if (extLen > HUMAN68K_EXT_MAX) {
    nameLen += extLen;
    extLen = 0;
  }
  if (nameLen > HUMAN68K_NAME_MAX) return false;

  const char* prefix = DEFAULT_DRV_CLN;
  const size_t prefixLen = strlen(DEFAULT_DRV_CLN);
  strcpy(hpn->path, prefix);
  strncpy(hpn->path + prefixLen, fullpath, pathLen);
  hpn->path[prefixLen + pathLen] = '\0';

  to_backslash(hpn->path);
  strcpy(hpn->name, name);

  hpn->nameLen = nameLen;
  hpn->extLen = extLen;

  return true;
}

// パス名の正規化
//   realpath()はシンボリックリンクを展開してしまうので使わない
bool CanonicalPathName_generic(const char* path, Human68kPathName* hpn) {
  char buf[PATH_MAX];

  if (!absolutePath(path, sizeof(buf), buf)) return false;
  return canonical_pathname(buf, hpn);
}
#endif

#ifdef HOST_ADD_LAST_SEPARATOR_GENERIC
// パス名の末尾にパスデリミタを追加する
void AddLastSeparator_generic(char* path) {
  size_t len = strlen(path);
  if (len == 0 || path[len - 1] != '/') strcpy(path + len, "/");
}
#endif

#ifdef HOST_PATH_IS_FILE_SPEC_GENERIC
// 文字列にパス区切り文字が含まれないか(ファイル名だけか)を調べる
//   true -> ファイル名のみ
//   false -> "/" が含まれる
bool PathIsFileSpec_generic(const char* path) {
  return strchr(path, '/') == NULL;
}
#endif

#ifdef HOST_GET_STANDARD_HOSTFILE_GENERIC
static FILE* fileno_to_fp(int fileno) {
  if (fileno == HUMAN68K_STDIN) return stdin;
  if (fileno == HUMAN68K_STDOUT) return stdout;
  if (fileno == HUMAN68K_STDERR) return stderr;
  return NULL;
}

// 標準入出力ファイルに関するFINFO構造体の環境依存メンバーを返す。
HostFileInfoMember GetStandardHostfile_generic(int fileno) {
  HostFileInfoMember hostfile = {fileno_to_fp(fileno)};
  return hostfile;
}
#endif

static int toDosError(int e, int defaultError) {
  switch (e) {
    default:
      break;

    case EEXIST:
      return DOSE_EXISTFILE;
    case EACCES:
      return DOSE_RDONLY;
    case EISDIR:
      return DOSE_ISDIR;
    case EMFILE:
    case ENFILE:
      return DOSE_MFILE;
    case ENOENT:
      return DOSE_NOENT;
    case ENOMEM:
      return DOSE_NOMEM;
    case ENOSPC:
      return DOSE_DISKFULL;
  }

  return defaultError;
}

static inline int toHostFilename(char* fullpath, char* buf,
                                 size_t sizeofBuf) {
  char* p = fullpath + getDriveNameLen(fullpath);
  to_slash(p);
  return HOST_CONVERT_FROM_SJIS(p, buf, sizeofBuf) ? 1 : 0;
}

#ifdef HOST_CREATE_NEWFILE_GENERIC
// ファイルを作成する
Long CreateNewfile_generic(char* fullpath, HostFileInfoMember* hostfile,
                           bool newfile) {
  char hostpath[HUMAN68K_PATH_MAX * 4 + 1];
  if (!toHostFilename(fullpath, hostpath, sizeof(hostpath)))
    return DOSE_ILGFNAME;

  struct stat st;
  if (stat(hostpath, &st) == 0) {
    if (S_ISCHR(st.st_mode)) {
      return DOSE_ILGFNAME;  // 同名のキャラクタデバイスが存在する
    }
    if (newfile) {
      return DOSE_EXISTFILE;  // 同名のファイルが既に存在している
    }
  }

  if (newfile) {
    FILE* fp = fopen(hostpath, "rb");
    if (fp != NULL) {
      fclose(fp);
      return toDosError(errno, DOSE_ILGFNAME);
    }
  }

  FILE* fp = fopen(hostpath, "w+b");
  if (fp == NULL) return toDosError(errno, DOSE_ILGFNAME);
  hostfile->fp = fp;
  return 0;
}
#endif

#ifdef HOST_OPEN_FILE_GENERIC
Long OpenFile_generic(char* fullpath, HostFileInfoMember* hostfile,
                      FileOpenMode mode) {
  char hostpath[HUMAN68K_PATH_MAX * 4 + 1];
  if (!toHostFilename(fullpath, hostpath, sizeof(hostpath)))
    return DOSE_ILGFNAME;

  static const char mdstr[][4] = {"rb", "r+b", "r+b"};
  FILE* fp = fopen(hostpath, mdstr[mode]);
  if (fp == NULL) return toDosError(errno, DOSE_NOENT);

  // ディレクトリをオープンしてしまった可能性がある。
  struct stat st;
  if (fstat(fileno(fp), &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      fclose(fp);
      return DOSE_ISDIR;
    }
  }

  hostfile->fp = fp;
  return 0;
}
#endif

#ifdef HOST_CLOSE_FILE_GENERIC
// ファイルを閉じる
bool CloseFile_generic(FILEINFO* finfop) {
  FILE* fp = finfop->host.fp;
  if (fp == NULL) return false;

  finfop->host.fp = NULL;
  return fclose(fp) == EOF ? false : true;
}
#endif

#ifdef HOST_READ_FILE_OR_TTY_GENERIC
#include <unistd.h>

// 端末からの入力
static Long read_from_tty(char* buffer, ULong length) {
  ULong read_len = gets2(buffer, length);
  int crlf_len = ((length - read_len) >= 2) ? 2 : length - read_len;
  memcpy(buffer + read_len, "\r\n", crlf_len);
  return read_len + crlf_len;
}

// ファイル読み込み
Long ReadFileOrTty_generic(FILEINFO* finfop, char* buffer, ULong length) {
  if (isatty(fileno(finfop->host.fp))) return read_from_tty(buffer, length);

  return (Long)fread(buffer, 1, length, finfop->host.fp);
}
#endif

#ifdef HOST_SEEK_FILE_GENERIC
Long SeekFile_generic(FILEINFO* finfop, Long offset, FileSeekMode mode) {
  static const int seekModes[] = {SEEK_SET, SEEK_CUR, SEEK_END};

  if (fseek(finfop->host.fp, offset, seekModes[mode]) != 0)
    return DOSE_CANTSEEK;

  return ftell(finfop->host.fp);
}
#endif

static void not_implemented(const char* name) {
  printFmt("run68: %s()は未実装です。\n", name);
}

#ifdef HOST_DOS_MKDIR_GENERIC
// DOS _MKDIR (0xff39) (未実装)
Long DosMkdir_generic(Long name) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}
#endif

#ifdef HOST_DOS_RMDIR_GENERIC
// DOS _RMDIR (0xff3a) (未実装)
Long DosRmdir_generic(Long name) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}
#endif

#ifdef HOST_DOS_CHDIR_GENERIC
// DOS _CHDIR (0xff3b) (未実装)
Long DosChdir_generic(Long name) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}
#endif

#ifdef HOST_DOS_CURDIR_GENERIC
#include <unistd.h>

// DOS _CURDIR (0xff47)
Long DosCurdir_generic(short drv, char* buf_ptr) {
  char buf[PATH_MAX];
  const char* p = getcwd(buf, sizeof(buf));
  if (p == NULL) {
    // Human68kのDOS _CURDIRはエラーコードとして-15しか返さないので
    // getdcwd()が失敗する理由は考慮しなくてよい。
    return DOSE_ILGDRV;
  }
  if (!HOST_CONVERT_TO_SJIS(buf + ROOT_SLASH_LEN, buf_ptr,
                            (HUMAN68K_DIR_MAX - 1) + 1)) {
    return DOSE_ILGDRV;
  }
  to_backslash(buf_ptr);
  return 0;
}
#endif

#ifdef HOST_DOS_FILEDATE_GENERIC
// DOS _FILEDATE (0xff57, 0xff87) (未実装)
Long DosFiledate_generic(UWord fileno, ULong dt) {
  not_implemented(__func__);
  return DOSE_ILGFNC;
}
#endif

#ifdef HOST_IOCS_ONTIME_GENERIC
// IOCS _ONTIME (0x7f)
RegPair IocsOntime_generic(void) {
  const int SECS_PER_DAY = 24 * 60 * 60;  // 1日の秒数
  const int CS_PER_SEC = 100;             // 1秒 = 100センチ秒

  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return (RegPair){0, 0};

  // 経過日数
  ULong days = ts.tv_sec / SECS_PER_DAY;

  // 1日未満の残り秒数を100分の1秒単位に変換
  ULong csInDay = (ts.tv_sec % SECS_PER_DAY) * CS_PER_SEC;

  // ナノ秒を100分の1秒単位に変換して加算
  csInDay += ts.tv_nsec / 10000000L;

  return (RegPair){csInDay, days & 0xffff};
}
#endif
