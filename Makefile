DEBUG=false


CFLAGS = -Os -Winline -Wall -Wextra \
  -fno-plt -fno-stack-protector -fno-stack-check -fno-unwind-tables -ffast-math \
  -fno-asynchronous-unwind-tables -fomit-frame-pointer -fsingle-precision-constant -fno-pie -no-pie \
  -fno-pic -fno-PIE -fno-PIC -march=nocona -ffunction-sections -fdata-sections \
  -fmerge-all-constants -mno-fancy-math-387 -mno-ieee-fp -std=gnu11 -nostartfiles

ifeq ($(DEBUG),false)
	CFLAGS += #-nostdlib # needed for rand()
else
	CFLAGS += -DDEBUG=true -g
	LDFLAGS += -g
endif

CFLAGS += -Wl,--gc-sections,--no-keep-memory,--no-export-dynamic,--orphan-handling=discard,-z,noseparate-code,-z,stack-size=0,--hash-style=gnu,-Tlinker.ld,-z,max-page-size=64,-nostdlib,--build-id=none
#TODO ,--no-dynamic-linker

.PHONY: clean checkgccversion noelfver

all : checkgccversion Minecraft4k

noelfver:
	make -C noelfver

packer : vondehi/vondehi.asm 
	cd vondehi; nasm -fbin -o vondehi vondehi.asm -DNO_CHEATING

shader.h : shader.frag Makefile
	mono ./shader_minifier.exe --preserve-externals shader.frag -o shader.h

Minecraft4k.elf : Minecraft4k.c shader.h linker.ld Makefile
	gcc -o $@ $< $(CFLAGS)

Minecraft4k : Minecraft4k_opt.elf.packed
	mv $< $@

#all the rest of these rules just takes a compiled elf file and generates a packed version of it with vondehi
%_opt.elf : %.elf Makefile noelfver
	cp $< $@
	strip -R .note.gnu.property -R .note.gnu.build-id -R .gnu.hash -R .gnu.version -R .fini -R .init_array -R .got -R .discard -R .eh_frame -R .got.plt -R .comment $@
	# ./Section-Header-Stripper/section-stripper.py $@
	sstrip -z $@
	# ./noelfver/noelfver $@ > $@.nover
	# mv $@.nover $@
	
	truncate -s -50 $@

	sstrip $@

	# clear out useless bits to improve compression
	@sed -i 's/_edata/\x00\x00\x00\x00\x00\x00/g' $@;
	@sed -i 's/__bss_start/\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00/g' $@;
	@sed -i 's/_end/\x00\x00\x00\x00/g' $@;
	@sed -i 's/GLIBC_2\.2\.5/\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00/g' $@;
	@sed -i 's/GLIBC_2\.34/\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00/g' $@;

	# risky!
	@sed -i 's/ELF\x02\x01\x01/ELF\x00\x00\x00/g' $@; # rip elf header lol
	@sed -i 's/Qåtd/\x00\x00\x00\x00/g' $@;
	@sed -i 's/}}\x00\x90/\x7D\x7D\x00\x00/g' $@;
	@sed -i 's/H\x83\xEChH/H\x83\xECHH/g' $@;
	@printf '\x00' | dd of=$@ bs=1 seek=20 count=1 conv=notrunc >&/dev/null # entry point... why is this ok?? 
	@printf '\x00' | dd of=$@ bs=1 seek=52 count=1 conv=notrunc >&/dev/null
	@# sed -i 's/\x04\x00\x00\x00\x40/\x00\x00\x00\x00\x00/g' $@;
	@printf '\x00\x00\x00' | dd of=$@ bs=1 seek=88 count=3 conv=notrunc >&/dev/null
	@printf '\x00\x00' | dd of=$@ bs=1 seek=96 count=2 conv=notrunc >&/dev/null
	@printf '\x00\x00' | dd of=$@ bs=1 seek=104 count=2 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=112 count=1 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=124 count=1 conv=notrunc >&/dev/null
	@printf '\x00\x00' | dd of=$@ bs=1 seek=136 count=2 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=146 count=1 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=160 count=1 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=168 count=1 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=202 count=1 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=216 count=1 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=224 count=1 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=236 count=1 conv=notrunc >&/dev/null
	@printf '\x00\x00' | dd of=$@ bs=1 seek=240 count=2 conv=notrunc >&/dev/null
	@printf '\x00\x00\x00' | dd of=$@ bs=1 seek=256 count=3 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=264 count=1 conv=notrunc >&/dev/null
	@printf '\x00\x00' | dd of=$@ bs=1 seek=272 count=2 conv=notrunc >&/dev/null
	@printf '\x00' | dd of=$@ bs=1 seek=280 count=1 conv=notrunc >&/dev/null
	@printf '\x00\x00\x00\x00\x00' | dd of=$@ bs=1 seek=288 count=5 conv=notrunc >&/dev/null

	chmod +x $@

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
