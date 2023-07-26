cmake_minimum_required(VERSION 3.16)
project(run68 C)

if(NOT MSVC)
  option(USE_ICONV "Use iconv for converting Shift-JIS to UTF-8." ON)
endif()

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
  src/calc.c
  src/conditions.c
  src/debugger.c
  src/disassemble.c
  src/dos_memory.c
  src/dos_memory.h
  src/doscall.c
  src/eaaccess.c
  src/exec.c
  src/getini.c
  src/human68k.h
  src/hupair.c
  src/hupair.h
  src/key.c
  src/iocscall.c
  src/line0.c
  src/line2.c
  src/line4.c
  src/line5.c
  src/line6.c
  src/line7.c
  src/line8.c
  src/line9.c
  src/lineb.c
  src/linec.c
  src/lined.c
  src/linee.c
  src/linef.c
  src/load.c
  src/mem.c
  src/mem.h
  src/run68.c
  src/run68.h
)

if(WIN32)
  target_sources(${PROJECT_NAME} PRIVATE
    src/ansicolor-w32.c
    src/ansicolor-w32.h
    src/host_win32.c
    src/host_win32.h
  )
else()
  target_sources(${PROJECT_NAME} PRIVATE
    src/host_generic.c
    src/host_generic.h
  )
endif()

target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /source-charset:utf-8 /J)

  # PathAddBackslashA()
  target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -funsigned-char -O3 -Wall -Wextra -Werror -Wno-unused-parameter)

  # linef.c uses math functions.
  target_link_libraries(${PROJECT_NAME} PRIVATE m)
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
