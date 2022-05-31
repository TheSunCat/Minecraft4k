/*
 * Copyright (c) 2019 PoroCYon
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the LICENSE file for more details.
 */

static int PATCHUP_NAME(Elf64_Ehdr* elf) {
    for (size_t i = 0; i < elf->e_phnum; ++i) {
        Elf64_Phdr* phdr = (Elf64_Phdr*)(
            (uint8_t*)elf + elf->e_phoff + i * elf->e_phentsize);

        if (phdr->p_type != PT_DYNAMIC) {
            continue;
        }

        Elf64_Dyn* dynbase = (Elf64_Dyn*)((uint8_t*)elf + phdr->p_offset);

        for (size_t j = 0; ; ++j) {
            Elf64_Dyn* dyn = &dynbase[j];

            if (dyn->d_tag == DT_NULL) break;

            for (size_t k = 0; k < lengthof(dyn_forbidden); ++k) {
                if (dyn->d_tag == dyn_forbidden[k]) {
                    if (dynbase[j+1].d_tag == DT_NULL) {
                        dyn->d_tag = DT_NULL;
                    } else {
                        // TODO: instead of replacing it with a placeholder
                        // value, move the following entries up, and overwrite
                        // the end with zeros.
                        dyn->d_tag = DT_DEBUG; // placeholder value
                    }
                    dyn->d_un.d_val = 0;
                }
            }
        }
    }

    return 0;
}

