/*
 * Copyright (c) 2019 PoroCYon
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the LICENSE file for more details.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <elf.h>

static void print_help(char* arg0) {
    printf(
"%s: remove symbol versioning information from the DYNAMIC segment of an ELF\n"
"file. Does NOT remove the sections containing the actual versioning\n"
"information, use something like `objcopy -R .gnu.version -R .gnu.version_r`\n"
"to remove those.\n"
"\n"
"Usage:\n"
"\t%s -h|--help\n"
"\t\tPrint this text.\n"
"\n"
"\t%s input.elf > output.elf\n"
"\t\tPatch the input.elf file, output will be written to stdout.\n"
"\t\tIf stdout is a TTY, this text will be displayed as well.\n"
    , arg0, arg0, arg0);
}

static const int dyn_forbidden[] = {
    DT_VERSYM    ,
    DT_VERDEF    ,
    DT_VERDEFNUM ,
    DT_VERNEED   ,
    DT_VERNEEDNUM,
};

#define lengthof(x) (sizeof(x)/sizeof(*x))

// ¯\_(ツ)_/¯
#define PATCHUP_NAME patch_up_elf64
#include "patchup.h"
#undef PATCHUP_NAME

#define PATCHUP_NAME patch_up_elf32
#define Elf64_Ehdr Elf32_Ehdr
#define Elf64_Phdr Elf32_Phdr
#define Elf64_Dyn  Elf32_Dyn
#include "patchup.h"
#undef Elf64_Dyn
#undef Elf64_Phdr
#undef Elf64_Ehdr
#undef PATCHUP_NAME

int main(int argc, char* argv[]) {
    if (argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")
            || isatty(STDOUT_FILENO)) {
        print_help(argv[0]);
        return isatty(STDOUT_FILENO) ? 1 : 0;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        dprintf(STDERR_FILENO, "Can't open input file '%s'.\n", argv[1]);
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        dprintf(STDERR_FILENO, "Can't stat file '%s'.\n", argv[1]);
        close(fd);
        return 1;
    }

    void* mem = malloc((size_t)st.st_size);

    if (read(fd, mem, (size_t)st.st_size) != st.st_size) {
        dprintf(STDERR_FILENO, "Can't read from file '%s'.\n", argv[1]);
        free(mem);
        close(fd);
        return 1;
    }

    close(fd);
    int rv = 0;

    bool is32bit;

    // try to emulate 'bugs' in the kernel a little -- on x86es, it only looks
    // at e_machine, never at EI_CLASS.
    if (((Elf32_Ehdr*)mem)->e_machine == EM_386) {
        is32bit = true;
    } else if (((Elf64_Ehdr*)mem)->e_machine == EM_X86_64) {
        is32bit = false;
    } else {
        is32bit = ((uint8_t*)mem)[EI_CLASS] < ELFCLASS64;
    }

    int res = (is32bit ?
        patch_up_elf32((Elf32_Ehdr*)mem) :
        patch_up_elf64((Elf64_Ehdr*)mem));
    if (res) {
        rv = res;
        goto ERR;
    }

    write(STDOUT_FILENO, mem, (size_t)st.st_size);

ERR:
    free(mem);

    return rv;
}

