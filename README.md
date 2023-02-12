# cliloc
Simple cliloc &lt;-> csv converter for Ultima Online. If you don't know what that means you won't need it ;-). Wrote my own, because I didn't like any of the existing converters/tools.

Cliloc is a simple file format used to store loca string used by old-school Ultima Online clients.

The files consist of a header (seemlingly always **{ 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 }**), followed by a list of Entries. With each entry consisting of **Id** (u32), **Flags** (u8), **Length** (u16), **String** (Length * char). 

Note, this tool has only be build and tested on a Linux system, with GCC 12. Other systems (Windows, MacOS) and compilers (clang, msvc) **should** also work, but might need minor tweaks.

# Building

On a Linux system with gcc installed build.sh will do everything you need (``mkdir -p bin && gcc -Wall -Werror src/main.c -o bin/cliloc``).
