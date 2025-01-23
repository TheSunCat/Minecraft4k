cmake_minimum_required(VERSION 3.14)
project(Minecraft4k LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb")

add_custom_command(
    OUTPUT shader.h
    COMMAND mono ${CMAKE_SOURCE_DIR}/shader_minifier.exe --preserve-externals ${CMAKE_SOURCE_DIR}/shader.frag -o shader.h
    DEPENDS shader.frag
    COMMENT "Generating shader.h from shader.frag"
)

add_executable(Minecraft4k Minecraft4k.c Constants.h shader.h)

target_include_directories(Minecraft4k PRIVATE ${CMAKE_BINARY_DIR})

# NOTE: -ffunction-sections crashes
target_compile_options(Minecraft4k PRIVATE -m32 -Os -Winline -Wall -Wextra -Wno-incompatible-pointer-types -fno-pie -no-pie -fno-plt -fno-stack-protector -ffast-math -march=nocona -nostartfiles -fdata-sections)

target_link_options(Minecraft4k PRIVATE -m32 -T "${CMAKE_SOURCE_DIR}/linker.ld" -z max-page-size=64 -nostartfiles --data-sections -nodefaultlibs -Wl,--gc-sections)

# needed for dlsym
target_link_libraries(Minecraft4k PRIVATE c)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(
        TARGET Minecraft4k POST_BUILD
        COMMAND cp $<TARGET_FILE:Minecraft4k> $<TARGET_FILE:Minecraft4k>.debug
        COMMENT "Saving debug build"
    )
endif()

add_custom_command(
    TARGET Minecraft4k POST_BUILD
    COMMAND patchelf --shrink-rpath $<TARGET_FILE:Minecraft4k>
    COMMAND ${CMAKE_STRIP} --remove-section=.note.gnu.property --remove-section=.note.gnu.build-id --remove-section=.gnu.hash --remove-section=.fini --remove-section=.init_array --remove-section=.got --remove-section=.discard --remove-section=.eh_frame --remove-section=.got.plt --remove-section=.comment $<TARGET_FILE:Minecraft4k>
    COMMAND strip -s $<TARGET_FILE:Minecraft4k>
    COMMAND sstrip -z $<TARGET_FILE:Minecraft4k>
    COMMAND truncate -s -42 $<TARGET_FILE:Minecraft4k> # NOTE: tweak (lower) if it crashes
    COMMENT "Optimizing Minecraft4k"
)

find_package(Python REQUIRED)

set(VONDEHI_PATH ${CMAKE_SOURCE_DIR}/vondehi)
add_custom_command(
    TARGET Minecraft4k POST_BUILD
    COMMAND mv $<TARGET_FILE:Minecraft4k> $<TARGET_FILE:Minecraft4k>.unpacked
    COMMAND ${Python_EXECUTABLE} ${VONDEHI_PATH}/autovndh.py -x -l -Z --vndh=${VONDEHI_PATH} $<TARGET_FILE:Minecraft4k>.unpacked $<TARGET_FILE:Minecraft4k>
    COMMAND chmod +x $<TARGET_FILE:Minecraft4k>
    COMMAND wc -c $<TARGET_FILE:Minecraft4k>
    COMMENT "Packing with vondehi"
)
