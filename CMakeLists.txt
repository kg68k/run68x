
cmake_minimum_required(VERSION 3.13)

project(run68 C)
set(CMAKE_C_FLAGS 
    "-Wno-int-conversion -Wno-return-type -Wno-switch -Wno-implicit-function-declaration -Wno-format -Wno-invalid-source-encoding -Wno-pointer-integer-compare -Wno-int-to-pointer-cast -Wno-pointer-sign -Wno-incompatible-pointer-types")

set(SRC 
    src/ansicolor-w32.c
    src/calc.c
    src/conditions.c
    src/debugger.c
    src/disassemble.c
    src/eaaccess.c
    src/exec.c
    src/getini.c
    src/key.c
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
    src/iocscall.c
    src/doscall.c
    src/run68.h
)

add_executable(run68 ${SRC})
target_link_libraries(run68 PUBLIC m)
