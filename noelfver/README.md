# noelfver

Removes versioning information offsets from the PT_DYNAMIC phdr (segment)
from ELF files. Does ***NOT*** remove the actual versioning information (use
`objcopy -R .gnu.version -R .gnu.version_r` for that). Don't use this unless
you know what you are doing.

## If something breaks

It's entirely your fault. No warranty implied etc. etc. etc. I wrote this in
half an hour, so what do you expect?

## License

[WTFPL 2.0](/LICENSE)

