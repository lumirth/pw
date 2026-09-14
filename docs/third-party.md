# Third-party material and tools

The [references](references.md) describe the research and tools that inform this project.

The compiler archive reader in `tools/installshield.py` uses the InstallShield format and
block-decoding approach documented by [ISx](https://github.com/lifenjoiner/ISx)
and [Unshield](https://github.com/twogood/unshield). Their MIT notices follow.

[Wibo](https://github.com/decompals/wibo) is downloaded separately from its
upstream release, or supplied locally. uv, Ninja, Pillow, Ruff, clang-format and Cppcheck retain their own licenses.
The Python dependencies are recorded in `uv.lock`.

The NCG/NCL converter follows public format descriptions in
[NitroPaint](https://github.com/Garhoogin/NitroPaint) and
[Tinke](https://github.com/pleonex/tinke). The [artwork guide](../assets/README.md)
links the consulted revisions and explains the reconstructed editor metadata.

## Renesas HEW device header

[`include/startup/iodefine.h`](../include/startup/iodefine.h) is the retained
Renesas HEW device-register template. It identifies itself as **H8/38602 Series
Include File, Ver 2.1**, with the marker **HEW_2006.10.05**. It provides the
memory-mapped register declarations used by the firmware.

This vendor template is outside the project's CC0 dedication. Its existing
identification is retained in the file, and automated formatting and line-ending
conversion exclude it. The retained file contains no standalone license grant;
the project adds no license grant over Renesas's material.

## ISx

MIT License

Copyright (c) 2017 lifenjoiner

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Unshield

Copyright (c) 2003 David Eriksson <twogood@users.sourceforge.net>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
