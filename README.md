# pw

`pw` is a source reconstruction (matching decompilation) of the Pokéwalker
firmware. With retail artwork and a supported Renesas HEW compiler
toolchain, it builds a binary identical to the original firmware. [^1]

It was made using thorough research combined with reverse-engineering/disassembly/decompilation of:
- The Pokéwalker's retail firmware binary
- The HeartGold/SoulSilver ROMs
- The Renesas compiler toolchains likely to have produced the firmware binary

Sources and supporting projects are collected in [references](docs/references.md).

Special thanks can be found [later in this README](#special-thanks).

## Usage

To build this repository, you must provide lawfully-obtained copies of the following:
- The Pokéwalker ROM / binary for extracting art assets [^2]
- The Renesas H8, H8S, H8SX family compiler toolchain. 

Instructions for acquiring these will not be provided here.

Versions of the H8, H8S, H8SX family compiler toolchain known to work are listed below:[^3]

- v6.02.01
- v6.02.02

Seeing as it may be possible to modify the code to make older toolchain versions work too, I suspect the compiler version may not be uniquely identifiable, at least based on evidence currently available.

## Build

At time of writing, the project has been built successfully on:[^4]
- macOS Arm64 (M1 MacBook Pro Late 2020) 
- Windows x86_64 (Intel NUC i3).

The compiler executables are 32-bit PE Windows executables. They run natively on Windows x86_64 and through Wibo on macOS Arm64.

### Initial Setup

Install [uv](https://docs.astral.sh/uv/getting-started/installation/). It manages Python and the project's locked build dependencies. macOS Arm64 also needs [Rosetta](docs/build.md#host-setup) to run the Renesas tools through Wibo.

Import your compiler installation, updater executable, or supported updater ZIP:

```sh
uv run -m tools.toolchain import path/to/compiler
```

For the retail artwork, extract your ROM once:

```sh
uv run -m tools.assets extract path/to/retail.bin
```

This saves six NCG sprites, a BMP font sheet, and an editor palette in `assets/local/`. The build uses those files thereafter. You can skip extraction to use the placeholders.

### Actually Building

Configure and build:

```sh
uv run configure.py
uv run ninja
```

The firmware appears at `build/<toolchain>/pw.bin`.

Note:
- The only things necessary for a successful build are the imported compiler and artwork files (either placeholders or extracted from the Pokéwalker binary). After you've imported those, the original ROM and installer can be moved or removed.
- The project is set up such that you can use an unknown compiler version. The build tools will warn you if the compiler is not recognized, but otherwise let you proceed.
- To require the build to match the retail hash, run `uv run ninja verify`. To remove generated outputs while keeping your imported compiler and artwork, run `uv run ninja -t clean`.


See [build instructions](docs/build.md) for compiler selection, offline use, recovery, and development checks. See [artwork inputs](assets/README.md) for editing NCG files and the font.

## Layout

| Path | Purpose |
| --- | --- |
| `src/startup/`, `include/startup/` | H8 startup, interrupt entry points, and hardware declarations |
| `src/support/`, `include/support/` | Communication, shared helpers, and scratch allocation |
| `src/application/`, `include/application/` | Activities, device drivers, storage, and their interfaces |
| `src/globals.c`, `include/` | Global storage and shared types, records, resources, and view state |
| `assets/` | Editable input formats, placeholders, and ignored local artwork |
| `config/` | [Compiler options, memory placement, asset layouts, and tool identities](config/README.md) |
| `tools/` | Python extraction, conversion, build, and checking utilities |
| `tests/` | Authored fixtures checking the build utilities and recovery behavior |
| `docs/` | Building and [understanding the firmware](docs/source.md) |
| `.local/` | Ignored compiler installations, host tools, and machine configuration |
| `build/` | Generated firmware, maps, logs, and records of the inputs used |

`configure.py` discovers source files recursively and translates the small
configuration files into `build.ninja`. Modules link in alphabetical order by
their unique lowercase basenames. The [source guide](docs/source.md) explains
the naming and include conventions and provides reading paths through the
firmware.
Ninja's generated graph lists each output and dependency explicitly; edit the
configuration or generator when changing the build. The separate extraction
steps keep user-provided inputs reusable across clean builds.

## Source Policy

This project reconstructs the Pokéwalker firmware through reverse engineering and research.

Retail firmware images, extracted retail artwork, and the Renesas compiler toolchain are not distributed in this repository. Users provide those inputs locally, or use the included original placeholder artwork in place of retail assets.

Contributions must not include leaked or confidential source material, unauthorized copies of third-party code, or retail artwork.

This is an unofficial project and is not affiliated with or endorsed by Nintendo, The Pokémon Company, GAME FREAK, or Renesas.

## License

Except where otherwise noted, original material authored for this project is
released under [CC0 1.0 Universal](LICENSE). This dedication applies only to
rights held by the project's authors and does not grant rights in third-party
works or trademarks.

## FAQ

### Why are the files named the way they are?

Renesas's HEW, by default, links files in alphabetical order. Based on the groupings of functionality found in the retail binary, I decided to name the files accordingly, using prefixes to account for exceptions to the alphabetical order. 

It's possible the original project did not use the default linking behavior because—while a significant portion of the code is organized alphabetically by functionality (accelerometer preceding battery, then battle, etc)—there are exceptions to this order, specifically what we have labeled as the "startup" and "support" files. 

### Do you accept contributions?

The matching reconstruction is complete, so contributions are not a
priority. Suggestions and improvements to readability, organization,
documentation, and tooling are welcome. Source changes must follow the
existing conventions and preserve matching.

Please identify any third-party material in your contribution and retain
its applicable copyright and license notices. Do not submit leaked or
confidential development material, or third-party code or assets that
the project is not permitted to redistribute. Do not attach ROM dumps,
extracted retail artwork, or proprietary compiler binaries to issues or
pull requests.

Unless otherwise agreed, by submitting a contribution you agree to apply
the project's CC0 terms to the copyright and related rights you hold in
that contribution.

## Special Thanks

Thank you to:
- UnrealPowerz for their [pw_firm](https://github.com/UnrealPowerz/pw_firm/tree/main/) project. It was the first real public attempt at a decompilation, and it was what first inspired me to spend months working on this.
- h4lfheart for their [PocketWalker](https://github.com/h4lfheart/PocketWalker) emulator project. While not directly used or referenced, it was what originally got me to work on anything Pokéwalker-related at all.
- All the folks in the [Pokéwalker Hacking community Discord server](https://discord.gg/ymbTMsS). Could not have done it without them. `mamba2410`, `zenithknight`, `mriancamp`, `porocyon` and any other admins have done a great job of making a cool space to work in.
- Dmitry Grinberg for his [Pokéwalker hacking](https://dmitry.gr/?r=05.Projects&proj=28.+pokewalker) article. It's more-or-less foundational to the Pokéwalker hacking scene and it is an honor to be cited in it.
- Any and all authors in [references](docs/references.md).

There are many more folks who have contributed to the Pokéwalker hacking scene, and otherwise been instrumental (if not directly involved) in this project being able to succeed. I wish I could name each and every one. Thank you all for laying the groundwork that made this project possible. I hope this decomp can help further cool projects and community efforts going forward.

[^1]: This project assume's there's only one Pokéwalker firmware. There could presumably be multiple, possibly separated by region or production run. For the avoidance of doubt, this project refers to the original firmware with SHA-256 `f9e210a3b74afbbd12c5a66a51cc05cb9fbac986805ff0a3bfb4be6074d15607`.
[^2]: If you cannot obtain a copy of the ROM, you can still use this repository with the bundled permissively-licensed placeholders. Doing it this way means the code will not compile to the exact original firmware binary, but it will be usable as a reference.
[^3]: When I tested other versions, they did not work. It may be possible (or even likely) that older/newer compiler versions would work with modifications to the source code and build configuration. That said, after reviewing the evidence and the timeline of the Pokéwalker's release and development, I believe these versions are the ones most likely to have been used to build the original firmware.
[^4]: It may be possible to build on other platforms. These are just the ones I have been able to test.
