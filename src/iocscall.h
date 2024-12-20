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

#ifndef IOCSCALL_H
#define IOCSCALL_H

#include <time.h>

#include "run68.h"

typedef time_t (*TimeFunc)(time_t *);

ULong Datebcd(ULong bcd);
ULong Dateset(ULong bcd);
ULong Timebcd(ULong bcd);
ULong Timeset(ULong bcd);
ULong Dateget(TimeFunc timeFunc);
ULong Datebin(Long bcd);
ULong Timeget(TimeFunc timeFunc);
ULong Timebin(Long bcd);

bool iocs_call(void);

#endif
