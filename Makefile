DEBUG=false

CFLAGS = -Os -fno-plt -fno-stack-protector -fno-stack-check -fno-unwind-tables -ffast-math \
  -fno-asynchronous-unwind-tables -fomit-frame-pointer -fsingle-precision-constant -fno-pie -no-pie \
  -fno-pic -fno-PIE -fno-PIC -march=nocona -ffunction-sections -fdata-sections -fno-plt \
  -fmerge-all-constants -mno-fancy-math-387 -mno-ieee-fp -std=gnu11 -nostartfiles

#CFLAGS := -s -fno-plt -Os -std=gnu11 -nostartfiles -Wall -Wextra 
#CFLAGS += -fno-stack-protector -fno-stack-check -fno-unwind-tables -fno-asynchronous-unwind-tables -fomit-frame-pointer
#CFLAGS += -no-pie -fno-pic -fno-PIE -fno-PIC -march=core2 -ffunction-sections -fdata-sections
#CFLAGS += -fsingle-precision-constant -ffast-math 
#CFLAGS += -Wl,--gc-sections

ifeq ($(DEBUG),false)
	CFLAGS += -nostdlib
else
	CFLAGS += -DDEBUG=true -g
	LDFLAGS += -g
endif

CFLAGS += -Wl,--gc-sections,--no-keep-memory,--no-export-dynamic,--orphan-handling=discard,-z,noseparate-code,-z,stack-size=0
#TODO ,-z,max-page-size=256,--no-dynamic-linker,-nostdlib

.PHONY: clean checkgccversion noelfver

all : checkgccversion Minecraft4k

noelfver:
	make -C noelfver

packer : vondehi/vondehi.asm 
	cd vondehi; nasm -fbin -o vondehi vondehi.asm

shader.h : shader.frag Makefile
	mono ./shader_minifier.exe --preserve-externals shader.frag -o shader.h

Minecraft4k.elf : Minecraft4k.c shader.h Makefile
	gcc -o $@ $<  -lSDL2 -lGL $(CFLAGS)

Minecraft4k : Minecraft4k_opt.elf.packed
	mv $< $@

#all the rest of these rules just takes a compiled elf file and generates a packed version of it with vondehi
%_opt.elf : %.elf Makefile noelfver
	cp $< $@
	objcopy --remove-section .note.gnu.property --remove-section .gnu.version --remove-section .fini --remove-section .init_array --remove-section .got --remove-section .discard $@
	strip $@
	strip -R .note -R .comment -R .eh_frame -R .eh_frame_hdr -R .note.gnu.build-id -R .got -R .got.plt -R .gnu.version -R .shstrtab -R .gnu.version_r -R .gnu.hash -R .note.gnu.property $@
	./Section-Header-Stripper/section-stripper.py $@
	sstrip $@
	./noelfver/noelfver $@ > $@.nover
	mv $@.nover $@

	#clear out useless bits
	sed -i 's/_edata/\x00\x00\x00\x00\x00\x00/g' $@;
	sed -i 's/__bss_start/\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00/g' $@;
	sed -i 's/_end/\x00\x00\x00\x00/g' $@;
	sed -i 's/GLIBC_2\.2\.5/\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00/g' $@;

	chmod +x $@
	sstrip $@

%.xz : % Makefile
	-rm -f $@
	lzma --format=lzma -9 --extreme --lzma1=preset=9,lc=0,lp=0,pb=0,nice=40,depth=32,dict=16384 --keep --stdout $< > $@

%.packed : %.xz packer Makefile
	cat ./vondehi/vondehi $< > $@
	chmod +x $@
	wc -c $@
	cp Minecraft4k_opt.elf sMinecraft4k.elf

clean :
	-rm *.elf shader.h Minecraft4k
