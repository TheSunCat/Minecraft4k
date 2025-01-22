cmake_minimum_required(VERSION 3.14)
project(Minecraft4k LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_custom_command(
    OUTPUT shader.h
    COMMAND mono ${CMAKE_SOURCE_DIR}/shader_minifier.exe --preserve-externals ${CMAKE_SOURCE_DIR}/shader.frag -o shader.h
    DEPENDS shader.frag
    COMMENT "Generating shader.h from shader.frag"
)

add_executable(Minecraft4k Minecraft4k.c shader.h)

target_include_directories(Minecraft4k PRIVATE ${CMAKE_BINARY_DIR})
target_compile_options(Minecraft4k PRIVATE -m32 -Os -Winline -Wall -Wextra -Wno-incompatible-pointer-types -fno-pie -no-pie -fno-plt -fno-stack-protector -ffast-math -march=nocona -nostartfiles)

#TODO: --gc-sections --orphan-handling=discard --hash-style=sysv
target_link_options(Minecraft4k PRIVATE -m32 -T "${CMAKE_SOURCE_DIR}/linker.ld" -z max-page-size=64 -nostartfiles)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(Minecraft4k PRIVATE -ggdb)
else()
    target_compile_options(Minecraft4k PRIVATE )
endif()

if(FALSE)
add_custom_target(vondehi ALL
    COMMAND nasm -fbin -o ${CMAKE_BINARY_DIR}/vondehi ${CMAKE_SOURCE_DIR}/vondehi/vondehi.asm -DNO_CHEATING
    DEPENDS vondehi/vondehi.asm
    COMMENT "Assembling vondehi.asm"
)

add_custom_command(
    TARGET Minecraft4k POST_BUILD
    COMMAND ${CMAKE_STRIP} --remove-section=.note.gnu.property --remove-section=.note.gnu.build-id --remove-section=.gnu.hash --remove-section=.fini --remove-section=.init_array --remove-section=.got --remove-section=.discard --remove-section=.eh_frame --remove-section=.got.plt --remove-section=.comment $<TARGET_FILE:Minecraft4k>
    COMMAND sstrip -z $<TARGET_FILE:Minecraft4k>
    COMMAND truncate -s -50 $<TARGET_FILE:Minecraft4k>
    COMMENT "Optimizing Minecraft4k"
)

add_custom_command(
    TARGET Minecraft4k POST_BUILD
    # OUTPUT Minecraft4k.packed
    COMMAND xz --format=lzma -9 --extreme --lzma1=preset=9,lc=0,lp=0,pb=0,nice=40,depth=16,dict=8192,mf=bt2 --keep --stdout $<TARGET_FILE:Minecraft4k> > Minecraft4k.xz
    COMMAND cat vondehi Minecraft4k.xz > Minecraft4k.packed
    COMMAND truncate -s -4 Minecraft4k.packed
    COMMAND chmod +x Minecraft4k.packed
    DEPENDS Minecraft4k
    COMMENT "Creating packed version of Minecraft4k"
)
endif()
