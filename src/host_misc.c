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

#include <time.h>

#include "host.h"
#include "run68.h"

// IOCS _ONTIME (0x7f)
RegPair IocsOntime_win32(void) {
  time_t t = time(NULL);
  const int secondsPerDay = 24 * 60 * 60;
  return (RegPair){(t % secondsPerDay) * 100, (t / secondsPerDay) & 0xffff};
}
