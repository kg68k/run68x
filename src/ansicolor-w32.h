#pragma once

#ifdef _WIN32
#include <windows.h>

int WriteW32(short _type, HANDLE handle, const char* _buf, size_t _len);
#endif
