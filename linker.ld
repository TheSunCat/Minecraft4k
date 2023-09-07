OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64",
	     "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SEARCH_DIR("/usr/x86_64-pc-linux-gnu/lib64"); SEARCH_DIR("/usr/lib"); SEARCH_DIR("/usr/local/lib"); SEARCH_DIR("/usr/x86_64-pc-linux-gnu/lib");

SECTIONS
{
  PROVIDE (__executable_start = SEGMENT_START("text-segment", 0));
  . = SEGMENT_START("text-segment", 0x010000) + SIZEOF_HEADERS;

  .interp         : { *(.interp) }
  .gnu.version    : { *(.hash) *(.gnu.version) *(.gnu.version_r) }
  .dynstr         : { *(.dynstr) }
  .rela.got       : { *(.rela.got) }

  .text           : {
      *(.text.*)
      *(.rodata.*) *(.data.*)
      *(.got) *(.got.plt)
      *(.dynsym)
  }

  .dynamic        : { *(.dynamic) }
  .bss            : { *(.bss.*) }

  /DISCARD/       : {
    *(.note.gnu.build-id)
    *(.eh_frame)
  }
}
