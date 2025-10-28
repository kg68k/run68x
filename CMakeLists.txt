cmake_minimum_required(VERSION 3.22)
project(run68 C)

if(NOT MSVC)
  option(USE_ICONV "Use iconv for converting Shift-JIS to UTF-8." ON)
endif()

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
  src/conditions.c
  src/debugger.c
  src/disassemble.c
  src/dos_file.c
  src/dos_memory.c
  src/dos_misc.c
  src/doscall.c
  src/dostrace.c
  src/eaaccess.c
  src/exec.c
  src/fefunc.c
  src/getini.c
  src/host.c
  src/human68k.c
  src/hupair.c
  src/key.c
  src/iocscall.c
  src/line_0.c
  src/line_2.c
  src/line_4.c
  src/line_5.c
  src/line_6.c
  src/line_7.c
  src/line_8.c
  src/line_9.c
  src/line_b.c
  src/line_c.c
  src/line_d.c
  src/line_e.c
  src/line_f.c
  src/load.c
  src/mem.c
  src/run68.c
)
if(WIN32)
  target_sources(${PROJECT_NAME} PRIVATE
    src/ansicolor-w32.c
    src/host_win32.c
  )
endif()

target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /source-charset:utf-8 /execution-charset:shift_jis)
  target_compile_options(${PROJECT_NAME} PRIVATE /d1trimfile:${CMAKE_CURRENT_SOURCE_DIR}\\)
  target_compile_options(${PROJECT_NAME} PRIVATE /J)

  # PathAddBackslashA()
  target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -funsigned-char -O3 -Wall -Wextra -Werror -Wno-unused-parameter)

  # line_f.c uses math functions.
  target_link_libraries(${PROJECT_NAME} PRIVATE m)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
  # Avoid warnings in ctype functions
  target_compile_options(${PROJECT_NAME} PRIVATE -Wno-char-subscripts)
endif()

if(USE_ICONV)
  find_package(Iconv REQUIRED)
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ICONV)
  target_include_directories(${PROJECT_NAME} PRIVATE ${Iconv_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${Iconv_LIBRARIES})
endif()

if(MSYS)
  # To support utf-8 (cp932 is not a mistake. it works fine.)
  target_compile_options(${PROJECT_NAME} PRIVATE --exec-charset=cp932)
endif()

if(EMSCRIPTEN)
  target_link_options(${PROJECT_NAME} PRIVATE --embed-file ./fs@ -sFORCE_FILESYSTEM)
endif()

INSTALL(TARGETS run68 RUNTIME DESTINATION bin)
