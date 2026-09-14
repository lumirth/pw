# Building

Compiler import and artwork extraction are separate, one-time setup operations.
Compilation converts the extracted files and runs the imported tools. Its final
check uses the retail image's recorded size and SHA-256, so it works without a
ROM file.

## Host setup

The verified combinations are **macOS Arm64** through Wibo/Rosetta and
**Windows x86_64** with native compiler execution. Other combinations may work;
they remain unverified.

Install [uv](https://docs.astral.sh/uv/getting-started/installation/) through your
preferred package manager. For example, macOS with Homebrew:

```sh
brew install uv
softwareupdate --install-rosetta
```

On Windows with WinGet:

```powershell
winget install --id=astral-sh.uv -e
```

Run the remaining commands from the repository root. `uv run` prepares Python
and the dependencies recorded in `uv.lock`. The default Python series is 3.12;
the tools support Python 3.11 or newer. Ninja schedules compilation and Pillow
reads and writes BMP images. The C compiler remains user-provided.

On macOS, configuration downloads the Wibo version recorded in
`config/host-tools.json` and validates it before execution. To use an existing
launcher, pass `--wibo PATH` to `configure.py` or set `PW_WIBO`. To return to the
managed launcher, use `--managed-wibo`. Windows uses native execution.

Compiler processes run in separate scratch directories with short ASCII paths.
A checkout can contain spaces. If the system temporary directory is unsuitable,
pass `--work-dir PATH` naming an existing writable ASCII directory.

## Import a compiler

```sh
uv run -m tools.toolchain import path/to/compiler
```

The input can be a suite directory, its `bin` directory, `ch38.exe`, an enclosing
HEW installation containing one suite, an H8 updater executable, or a ZIP
containing one H8 updater. The importer reads archives directly and keeps the
compiler phases, runtime packs, and headers together in `.local/toolchains/`.
The original installation or installer is needed only during import.

These suites reproduce the retail image:

| Suite | CH38 | OPTLNK | Known package |
| --- | --- | --- | --- |
| 6.02.01 | 6.02.01 | 9.04.01 | `h8v6201u.exe` |
| 6.02.02 | 6.02.02 | 9.05.00 | `h8v6202u.exe` or `h8v6202u-doc-e.zip` |

The `h8v6202u-doc-e.zip` archive contains the updater and can be imported directly.
Importing an updater supplies a complete compiler suite without running the
installer or having an existing HEW installation.

`config/toolchains.json` records known package and installed-file hashes.
Unfamiliar fingerprints warn and remain usable. Missing necessary compiler
components or malformed archives stop import with a diagnostic. The matching
requirement applies to the produced image when strict verification is requested.

Use `--name LABEL` to give an import a distinct local name, especially when
trying another bundle of the same compiler revision:

```sh
uv run -m tools.toolchain import path/to/other/bundle --name experiment
uv run -m tools.toolchain list
uv run configure.py --toolchain experiment
```

Reimporting a name replaces that local installation after the new input has been
parsed and checked. A failed replacement restores the prior installation; if
restoration also fails, the error identifies the retained recovery directory.

## Extract artwork

```sh
uv run -m tools.assets extract path/to/retail.bin
```

The supported image is **49,152 bytes**, with SHA-256:

```text
f9e210a3b74afbbd12c5a66a51cc05cb9fbac986805ff0a3bfb4be6074d15607
```

Extraction checks this identity because the resource offsets describe that
image. It saves six NCG sprite files, a BMP font, and an NCL editor palette in
`assets/local/`. Existing files require `--force` before replacement. See the
[artwork guide](../assets/README.md) for formats, conversion and editing.

You can build before extraction: any missing image uses its original CC0
placeholder. An existing image with invalid dimensions or colors produces an
error identifying what needs correction. Placeholder use appears in the build
report and normally changes the output hash.

The editable containers preserve the recovered pixels and their packing. The
NCG writer supplies fresh editor metadata; the BMP font sheet is a reconstructed
representation of the glyphs.

## Compile and check

```sh
uv run configure.py
uv run ninja
```

Configuration selects the previous compiler, or the only available import. With
multiple imports, select one explicitly:

```sh
uv run configure.py --toolchain 6.02.01
uv run ninja
```

Outputs are written to `build/<toolchain>/`:

| Output | Contents |
| --- | --- |
| `pw.bin`, `pw.map` | Firmware image and linker map |
| `rom_assets.h` | Converted resident artwork arrays |
| `pw_pedometer.obj`, `pw_pedometer.log`, `pw_pedometer.json` | A module's object, compiler output, and input/output receipt; each C module has its own set |
| `runtime.log`, `link.log` | Runtime-library and linker output |
| `artwork.json`, `runtime.json`, `link.json`, `build.json` | Actual source, artwork, compiler, recipe and output hashes |
| `status.json` | Image size, SHA-256, retail-match result and placeholder use |

The object name comes from the C file's basename: for example,
`src/application/pw_pedometer.c` produces `build/<toolchain>/pw_pedometer.obj`.
The build discovers sources recursively and links their objects in alphabetical
basename order. Names must be unique and lowercase across the source tree.
Directory moves preserve that ordering; filename changes can affect the image.
See the [source guide](source.md#modules-and-reading-paths) for the module families.

A different output hash is a warning. A completed modified build returns success
and leaves `pw.bin` available. Compiler errors, invalid assets and inconsistent
build artifacts return failure with an actionable message.

For release verification, use:

```sh
uv run ninja verify
```

This requires the exact retail size and SHA-256 and writes `verified.json` on
success. Run it under **both 6.02.01 and 6.02.02** for accepted matching-source
changes. The build records also check that the image came from the current
source, assets and compiler files. They catch stale objects even when timestamps
have been preserved. Host tests and formatting complement this check.

## Clean, recover and work offline

Remove generated outputs for the selected compiler:

```sh
uv run ninja -t clean
uv run ninja
```

The clean command keeps `assets/local/`, imported compilers, and cached Wibo.
To clear another compiler's outputs, configure that compiler and run the same
clean command. `ninja -t clean` removes the outputs declared in the graph;
extra files saved manually in `build/` remain there.

Ninja rebuilds a changed C file individually; a header change rebuilds all
firmware units. Discovery includes nested source and header directories, and
source additions or removals regenerate the graph. Asset edits rebuild
conversion, `pw_builtin.c`, and the linked image. Adding or removing a local asset,
or restoring the local artwork directory, automatically updates its fallback
selection. Changing imported compiler files refreshes their recorded inventory
before the next build. A compiler whose required files have been removed needs
to be reimported.

After moving the checkout, rerun `uv run configure.py` to select the local
launcher and scratch paths. If a build reports obsolete or replaced artifacts,
refresh the graph before cleaning and rebuilding:

```sh
uv run configure.py
uv run ninja -t clean
uv run ninja
```

Compilation errors include the failing tool's log path. Fix the reported input
and repeat the build.

Once dependencies and Wibo are cached, the setup and build can run offline:

```sh
uv run --offline configure.py --offline
uv run --offline ninja
```

For an offline machine, prepare its Python environment and required host tools
in advance. The compiler and artwork import commands operate on local files.

## Development checks

The `dev` dependency group supplies clang-format 21.1.8 and Ruff. Install
Cppcheck 2.21.0 separately, then run:

```sh
uv run --group dev -m tools.check
```

This checks C formatting, Python formatting and lint, Cppcheck with the H8 target
model, and the utility tests. To apply formatting:

```sh
uv run --group dev -m tools.check --format
```

Formatting includes nested source and header directories. The accepted HEW
device header at `include/startup/iodefine.h` keeps its original formatting.
Cppcheck has two narrow suppressions for arithmetic whose evaluation follows
the target's 16-bit rules. To run only the utility tests:

```sh
uv run -m unittest discover -s tests
```

`uv` is a convenience for a reproducible host environment. An existing Python
environment with Ninja and Pillow can also run `python configure.py` and `ninja`;
the dependency versions are recorded in `uv.lock`.

## Why generate Ninja?

Ninja represents each build output and dependency explicitly. `tools/modules.py`
resolves the recursively discovered sources into one ordered module list used
by configuration, compilation, linking, and receipt validation.
`config/build.json` holds common options, per-module flag overrides, and section
placement. Common dependency groups keep repeated paths out of each
compiler command. See [Ninja's design](https://ninja-build.org/manual.html#_philosophical_overview).

Separate extraction and compilation are established decompilation workflows;
[SM64](https://github.com/n64decomp/sm64/blob/master/extract_assets.py) retains
extracted assets and [OOT](https://github.com/zeldaret/oot#4-setup-the-rom-and-build-process)
provides a separate asset-setup step. This project combines that workflow with
[uv's managed project environment](https://docs.astral.sh/uv/guides/projects/).
Ninja already supplies incremental builds and cleaning, so those tasks need no
additional task runner.
