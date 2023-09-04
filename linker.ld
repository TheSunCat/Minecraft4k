OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64",
	     "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SEARCH_DIR("/usr/x86_64-pc-linux-gnu/lib64"); SEARCH_DIR("/usr/lib"); SEARCH_DIR("/usr/local/lib"); SEARCH_DIR("/usr/x86_64-pc-linux-gnu/lib");

SECTIONS
{
  . = SEGMENT_START("text-segment", 0x010000) + SIZEOF_HEADERS;

  .interp         : { *(.interp) } /* address boundary error */
  .gnu.hash       : { *(.gnu.hash) } /* could not find section */
  .gnu.version    : { *(.gnu.version) *(.gnu.version_r) } /* could not find section */
  .dynsym         : { *(.dynsym) } /* could not find section */
  .dynstr         : { *(.dynstr) } /* address boundary error */
  .rela.dyn       : { *(.rela.got) } /* Address boundary error */

  .text           : { *(.text.*) }
  .rodata         : { *(.rodata.*) *(.data.*) } /* needed ?? */
  .dynamic        : { *(.dynamic) } /* Address boundary error */
  .got            : { *(.got) *(.got.plt) } /* segfault */
  .bss            : { *(.bss.*) }/* needed ?? */
}
