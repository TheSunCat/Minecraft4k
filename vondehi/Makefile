
default: test

testbin: test.c
	$(CC) -static -nostdlib -nostartfiles -O3 -s -o "$@" "$<"

%.gz: %
	< "$<" gzip -cnk9 > "$@"
%.lzma: %
	< "$<" lzma --format=lzma -9 --extreme --lzma1=preset=9,lc=1,lp=0,pb=0 --keep --stdout > "$@"

vondehi: vondehi.asm
	nasm -fbin -o"$@" "$<"
	chmod +x "$@"

test: vondehi testbin.lzma
	cat $^ > test && chmod +x test && strace -f ./test; echo $?

.PHONY: default test

