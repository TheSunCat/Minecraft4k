# vondehi

> "[four](https://vlasisku.alexburka.com/vo)-[dent](https://vlasisku.alexburka.com/denci)"

**An unpacker based on
[fishypack-trident](https://gitlab.com/PoroCYon/fishypack-trident/)
(which is my fork of
[Fishypack](https://bitbucket.org/blackle_mori/cenotaph4soda)),
but even smaller. It doesn't have a 64-bit version, though.**

## Comparison

| mode etc.    | vondehi | trident | Fishypack | sh-based unpacker |
|:------------ | -------:| -------:| ---------:| -----------------:|
| gzip, 32-bit |     159 |     172 | 179? (198?) |        48 to 72 |
| xz, 32-bit   | 162 (166*) |  179 |       186 |          48 to 72 |
| gzip, 64-bit |     N/A |     208 |      208? |          48 to 72 |
| xz, 64-bit   |     N/A |     217 |       217 |          48 to 72 |
| Preserves args |     Y |       N |  tries to | can, but often not |
| Min. platform | Linux 3.19 | Linux 2.27 | Linux 2.27 | Most Unices |
| Touches filesystem | N |       N |         N |                 Y |

\*: with `NO_UBUNTU_COMPAT` **dis**abled.

The exact size of a shell-based unpacker depends on the exact impmelentation,
many variations exist. 'xz' means the usage of `xzcat` instead of `zcat`,
the former supports both `xz`- and `lzma`-compressed data.

Fishypack and trident depend on Linux >=2.27 because of the use of the
`memfd_create` syscall. vondehi requires `execveat` as well.

Note that a 32-bit unpacker can still run a 64-bit binary, as long as the
kernel is 64-bit and supports the 32-bit emulation layer.

## Usage

```
nasm -fbin -o$out vondehi.asm [-DUSE_GZIP] [-DTAG="j0!"] [-DNO_UBUNTU_COMPAT] \
    [-DUSE_VFORK]
cat $out $intro_compressed > $final
```

### Settings

* `USE_GZIP` (default off): use `gzip` (`/bin/zcat`) instead of `xz`
  (`/usr/bin/xzcat`).
* `NO_UBUNTU_COMPAT` (default off): assume `/bin` is the same as `/usr/bin`.
  Originally named like this because on my machine, `/bin` is linked t
  `/usr/bin`, but on the Revision compomachine (which runs Ubuntu), it isn't.
* `USE_VFORK` (default off): use `vfork(2)` instead of `fork(2)`. I hope you
  know what you're doing when you enable this.
* `TAG` (default empty): add a vanity tag right before the compressed data.
  Only use this when you have bytes to spare, of course.

## How to debug it if it doesn't work

1. `strace` it
2. See where errors start happening
  * This can be obscured because the code assumes eg. syscall return values to
    be between `0` and `255`, so later syscalls might fail, or nonsense
    syscalls might be invoked.
3. Fix it. Somehow.

## Greets to

* Blackle, for the original Fishypack, and for replacing the `waitid(2)` call
  with `waitpid(2)`, fixing compatibility with some kernels and shaving off a
  few bytes at once!
* Shiz, for other packing/unpacking and x86-related stuff
* Faemiyah, yx, etc., for small sh-based unpackers (yx: nice trick with
  the script partially embedded in the gzip file!)

## License

[SAL](LICENSE).

