section-stripper.py is a utility to remove the section header from an ELF file, along with its string table. It also clears the fields in the ELF header related to the section header.

The standard unix utility `strip` doesn't do this, so this is why I created this file.

NOTE: this program assumes the section header's string table is roughly-contiguous with the section header itself. That is, they are both at the end of the file and only separated by a few (less than eight) bytes. If you get the error `Section header and its string table are not roughly contiguous` please open an issue with your file attached. I don't even think this happens in the wild so I didn't write the code to account for this.

## Install

```bash
pip install -r requirements.txt
```

## Usage:

```bash
./section-stripper.py my-program #strip file in-place
./section-stripper.py my-program my-program-stripped #output stripped file elsewhere (must be chmod'd to be executable)
cat my-program | ./section-stripper > my-program-stripped #do everything with pipes
```
