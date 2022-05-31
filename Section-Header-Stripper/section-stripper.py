#!/usr/bin/env python3

import sys
from typing import List, Tuple, BinaryIO, cast
from recordclass import recordclass
import struct

def binary_open(filename : str, mode : str) -> BinaryIO:
	return cast(BinaryIO, open(filename, mode + 'b'))

def get_files() -> Tuple[BinaryIO, BinaryIO]:
	inputfilepath = "/dev/stdin"
	outputfilepath = "/dev/stdout"

	argc = len(sys.argv)

	if argc > 1:
		inputfilepath = sys.argv[1]
		outputfilepath = inputfilepath
	if argc > 2:
		outputfilepath = sys.argv[2]

	if outputfilepath == inputfilepath:
		inputfile = binary_open(inputfilepath, 'r+')
		outputfile = inputfile
	else:
		inputfile = binary_open(inputfilepath, 'r')
		outputfile = binary_open(outputfilepath, 'w')

	return (inputfile, outputfile)

class BetterStruct:
	def __init__(self, fields : List[Tuple[str, str]], little_endian : bool) -> None:
		struct_fmt = ""
		struct_fields = ""

		if little_endian:
			struct_fmt += "<"
		else:
			struct_fmt += ">"

		for (fmt, name) in fields:
			struct_fmt += fmt + " "
			if not name is None:
				struct_fields += name + " "

		self._struct = struct.Struct(struct_fmt)
		self._tuple = recordclass("my_tuple", struct_fields)
		self.size = self._struct.size

	def unpack(self, data : bytes) -> None:
		self.fields = self._tuple._make(self._struct.unpack(data[:self.size]))

	def pack(self) -> bytes:
		return self._struct.pack(*self.fields)

if __name__ == "__main__":
	(inputfile, outputfile) = get_files()

	header_ident = BetterStruct([
		("4s", "magic"),
		("b", "arch"),
		("b", "endianness"),
		("b", "version"),
		("b", "abi"),
		("b", "abi_version"),
		("7x", None),
	], True)

	header_ident_bytes = inputfile.read(header_ident.size)
	header_ident.unpack(header_ident_bytes)

	if header_ident.fields.magic != b'\x7FELF':
		print("Input is not an ELF file.", file=sys.stderr)
		exit(1)

	if header_ident.fields.arch != 2:
		print("Only 64-bit ELF files are currently supported.", file=sys.stderr)
		exit(1)

	file_contents = header_ident.pack() + inputfile.read(-1)

	header = BetterStruct([
		("H", "type"),
		("H", "machine"),
		("L", "version"),
		("Q", "entry"),
		("Q", "phoff"),
		("Q", "shoff"),
		("L", "flags"),
		("H", "ehsize"),
		("H", "phentsize"),
		("H", "phnum"),
		("H", "shentsize"),
		("H", "shnum"),
		("H", "shstrndx"),
	], header_ident.fields.endianness == 1)

	header.unpack(file_contents[header_ident.size:])

	shoff = header.fields.shoff
	shentsize = header.fields.shentsize
	shnum = header.fields.shnum
	shstrndx = header.fields.shstrndx

	header.fields.shoff = 0
	header.fields.shentsize = 0
	header.fields.shnum = 0
	header.fields.shstrndx = 0

	end_of_section_header = shoff + shentsize*shnum

	file_contents = file_contents[:header_ident.size] + header.pack() + file_contents[header_ident.size+header.size:]

	shstr_entry = BetterStruct([
		("L", "name"),
		("L", "type"),
		("Q", "flags"),
		("Q", "addr"),
		("Q", "offset"),
		("Q", "size"),
		("L", "link"),
		("L", "info"),
		("Q", "addralign"),
		("Q", "entsize"),
	], header_ident.fields.endianness == 1)

	shstr_offset = shoff + shentsize*shstrndx
	shstr_entry.unpack(file_contents[shstr_offset:])

	SHT_STRTAB = 0x03
	if shstr_entry.fields.type != SHT_STRTAB:
		print("Can't find section header's string table.", file=sys.stderr)
		exit(1)

	end_of_strtable = shstr_entry.fields.offset + shstr_entry.fields.size
	if abs(shoff - end_of_strtable) > 8:
		print("Section header and its string table are not roughly contiguous.", file=sys.stderr)
		exit(1)

	if end_of_section_header != len(file_contents):
		print("Trailing bytes after end of section header.", file=sys.stderr)
		exit(1)

	outputfile.seek(0)
	outputfile.write(file_contents[:shstr_entry.fields.offset])
	outputfile.truncate()

	inputfile.close()
	outputfile.close()
