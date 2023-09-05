DEBUG=false


CFLAGS = -Os -Winline -Wall -Wextra \
  -fno-plt -fno-stack-protector -fno-stack-check -fno-unwind-tables -ffast-math \
  -fno-asynchronous-unwind-tables -fomit-frame-pointer -fsingle-precision-constant -fno-pie -no-pie \
  -fno-pic -fno-PIE -fno-PIC -march=nocona -ffunction-sections -fdata-sections \
  -fmerge-all-constants -mno-fancy-math-387 -mno-ieee-fp -std=gnu11 -nostartfiles  -m32

ifeq ($(DEBUG),false)
	CFLAGS += #-nostdlib # needed for rand()
else
	CFLAGS += -DDEBUG=true -g
	LDFLAGS += -g
endif

CFLAGS += -Wl,--gc-sections,--no-keep-memory,--no-export-dynamic,--orphan-handling=discard,-z,noseparate-code,-z,stack-size=0,--hash-style=gnu,-z,max-page-size=64,-nostdlib,--build-id=none
#TODO ,--no-dynamic-linker,-Tlinker.ld

.PHONY: clean checkgccversion noelfver

all : checkgccversion Minecraft4k

noelfver:
	make -C noelfver

packer : vondehi/vondehi.asm 
	cd vondehi; nasm -fbin -o vondehi vondehi.asm -DNO_CHEATING

shader.h : shader.frag Makefile
	mono ./shader_minifier.exe --preserve-externals shader.frag -o shader.h

Minecraft4k.elf : Minecraft4k.c shader.h linker.ld Makefile
	gcc -o $@ $<  -lGL -ldl $(CFLAGS)

Minecraft4k : Minecraft4k_opt.elf.packed
	mv $< $@

#all the rest of these rules just takes a compiled elf file and generates a packed version of it with vondehi
%_opt.elf : %.elf Makefile noelfver
	cp $< $@
	strip $@

	# this might crash, comment out if segfaulting
	strip -R .note.gnu.property -R .note.gnu.build-id -R .gnu.hash -R .gnu.version -R .fini -R .init_array -R .got -R .discard -R .eh_frame -R .got.plt -R .comment $@

# commented out as it only supports 64-bit :(
	#./Section-Header-Stripper/section-stripper.py $@
	sstrip -z $@
	./noelfver/noelfver $@ > $@.nover
	mv $@.nover $@

	#clear out useless bits
	sed -i 's/_edata/\x00\x00\x00\x00\x00\x00/g' $@;
	sed -i 's/__bss_start/\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00/g' $@;
	sed -i 's/_end/\x00\x00\x00\x00/g' $@;
	sed -i 's/GLIBC_2\.2\.5/\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00/g' $@;
	sed -i 's/GLIBC_2\.34/\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00/g' $@;
	sed -i 's/libc\.so\.6/\x00\x00\x00\x00\x00\x00\x00\x00\x00/g' $@;

	chmod +x $@
	sstrip $@

%.xz : % Makefile
	-rm -f $@
	lzma --format=lzma -9 --extreme --lzma1=preset=9,lc=0,lp=0,pb=0,nice=40,depth=16,dict=16384 --keep --stdout $< > $@

%.packed : %.xz packer Makefile
	cat ./vondehi/vondehi $< > $@

	# remove CRC32 (4 bytes)
	truncate -s -4 $@
	# truncate some more bytes (NOTE : unsafe, upon any segfaults just comment the next line)
	truncate -s -4 $@

	chmod +x $@
	wc -c $@
	cp Minecraft4k_opt.elf Minecraft4k_prepacked.elf

clean :
	-rm *.elf shader.h Minecraft4k
