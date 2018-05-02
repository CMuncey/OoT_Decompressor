# OoT_Decoder

---

This is a decoder for Zelda: Ocarina of Time. It will take a compressed ROM of the game, and decompress it into another ROM. This is useful for modifying the game data, as most sources detailing memory offsets are done so for the decompressed version of the game, not the compressed version.

This is essentially the same as another decoder be the name of ndec, but I wanted one of my own, so I made a new one. Mine uses the same algorithm, and some of the same files as ndec.

If you want to play the OoT Randomiser, you'll need a decompressed ROM, so you would want to put a compressed ROM through this program first, then use the randomiser.

---

Compiling on Linux/Mac: gcc -o Decode decoder.c

Compiling on Windows: gcc -o Decode.exe decoder.c -l ws2_32

You'll probably want MinGW for this if you're on Windows. That's what I used anyway.

If you don't know how to compile stuff on Windows (I sure didn't), oput the extractor folder on your desktop to make it easier to find, open up a command prompt (Press Windows, then type CMD), type cd Desktop/extractor (or whatever the extractor folder is called), then type the command above.

---

Usage: Decompress.exe [inputrom.z64]

If you don't know how to run programs on windows with arguments, it's the same process as compiling it, only you replace the gcc line with what's written above.

Or just drag and drop a compressed ROM into decompress.exe
