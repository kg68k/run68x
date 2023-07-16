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
  src/doscall.c
  src/eaaccess.c
  src/exec.c
  src/getini.c
  src/human68k.h
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
endif()

target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)

target_compile_definitions(${PROJECT_NAME} PRIVATE ENV_FROM_INI)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /source-charset:utf-8)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -O3 -Wno-int-conversion -Wno-return-type -Wno-switch -Wno-implicit-function-declaration -Wno-format -Wno-invalid-source-encoding -Wno-pointer-integer-compare -Wno-int-to-pointer-cast -Wno-pointer-sign -Wno-incompatible-pointer-types)

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
