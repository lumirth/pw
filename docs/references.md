# References

This document catalogs the sources consulted for this project and related
community projects.

Source-code links use the consulted revision where it is recorded. License notices for third-party tools appear in the [third-party acknowledgments](third-party.md).

## Pokéwalker research and game behavior

- Dmitry Grinberg's investigation of the hardware, firmware, infrared protocol,
  EEPROM contents, and device workflows. The accompanying annotated firmware
  disassembly and DECODEIMG utility helped interpret instructions and packed
  artwork.
  [PokéWalker hacking](https://dmitry.gr/?r=05.Projects&proj=28.%20pokewalker).
- Martin Korth's technical reference covering Pokéwalker commands, memory
  organization, peripherals, and cartridge communication.
  [GBATEK: Pokéwalker IR commands](https://problemkaputt.de/gbatek.htm#dscartinfraredpwalkerircommands).
- The memory-map tables used to investigate persistent records, EEPROM regions,
  resource boundaries, and their RAM counterparts.
  [GBATEK: Pokéwalker memory map](https://problemkaputt.de/gbatek.htm#dscartinfraredpwalkermemorymap).
- The register tables used to identify hardware accesses, including the RTC
  time registers.
  [GBATEK: H8/386 special-function registers](https://problemkaputt.de/gbatek.htm#h8386sfrs).
- nocash's firsthand account of reconstructing command tables and correcting
  packet lengths and memory-map interpretations, consulted during the
  persistent-record and protocol investigations.
  [NESdev Pokéwalker discussion](https://forums.nesdev.org/viewtopic.php?start=30&t=21140).
- The public reconstruction of HeartGold and SoulSilver, used to understand
  Pokémon data, saved records, serialization, and the console endpoint.
  [PRET pokeheartgold, consulted revision](https://github.com/pret/pokeheartgold/tree/0985e8718df4f25e64d6507d89c0c97c0d288981).
- The principal HGSS communication application, used to follow complete
  Pokéwalker transactions and their effects on game state.
  [PRET's Pokéwalker overlay](https://github.com/pret/pokeheartgold/blob/0985e8718df4f25e64d6507d89c0c97c0d288981/asm/overlay_112.s).
- Community reverse engineering covering infrared communication, captures,
  compression, and stored data.
  [mamba2410/reverse-pokewalker](https://github.com/mamba2410/reverse-pokewalker).
- Tools and experiments connecting HGSS under DeSmuME to a software Walker,
  together with EEPROM and artwork inspection utilities.
  [Pusty/PokewalkerUtils](https://github.com/Pusty/PokewalkerUtils).
- A public implementation useful for understanding how course data is
  assembled and selected for transfer.
  [Lincoln-LM's Pokéwalker course injector](https://github.com/lincoln-lm/pokewalker-course-injector/blob/43dc0617ea34bd4ac14e20ea0e64109a013d7f1b/main.py).

### PRET reading list

These paths were consulted alongside the communication overlay. Each link
uses revision `0985e8718df4f25e64d6507d89c0c97c0d288981`.

- Pokémon creation, field access, and gender and shininess calculations used
  to interpret transferred Pokémon records.
  [Pokémon operations](https://github.com/pret/pokeheartgold/blob/0985e8718df4f25e64d6507d89c0c97c0d288981/src/pokemon.c).
- Saved Trainer House records and the operations that manage them, used when
  following exchanged trainer and party data.
  [Trainer House storage](https://github.com/pret/pokeheartgold/blob/0985e8718df4f25e64d6507d89c0c97c0d288981/src/save_trainer_house.c).
- Trainer-profile initialization and accessors, used to follow identity
  values into communication records.
  [Player data](https://github.com/pret/pokeheartgold/blob/0985e8718df4f25e64d6507d89c0c97c0d288981/src/player_data.c).
- Pokémon and course gift processing, used to investigate distribution data
  and course unlocks.
  [Mystery Gift operations](https://github.com/pret/pokeheartgold/blob/0985e8718df4f25e64d6507d89c0c97c0d288981/src/scrcmd_mystery_gift.c).
- Inventory insertion and its success or failure result, used to establish
  the effect of return-trip item awards.
  [Bag operations](https://github.com/pret/pokeheartgold/blob/0985e8718df4f25e64d6507d89c0c97c0d288981/src/bag.c).
- Encoded-string copying and termination, used to follow names between
  communication records and game string objects.
  [String operations](https://github.com/pret/pokeheartgold/blob/0985e8718df4f25e64d6507d89c0c97c0d288981/src/pm_string.c).
- String buffering for messages, used to trace displayed text back to the
  values supplied by communication paths.
  [Message formatting](https://github.com/pret/pokeheartgold/blob/0985e8718df4f25e64d6507d89c0c97c0d288981/src/message_format.c).

## Hardware

- The microcontroller family's registers, memory organization, interrupts,
  timers, ADC, and serial peripherals.
  [Renesas H8/38602R Group Hardware Manual](https://www.renesas.com/en/document/mah/h838602r-group-hardware-manual).
- Device-specific additions needed when applying the family manual to the
  H8/38606.
  [Renesas H8/38606 addition note](https://www.renesas.com/en/document/tcu/addition-h838606-group).
- Instruction semantics, addressing modes, arithmetic, and processor flags
  used when interpreting the executable.
  [Renesas H8/300H Series Software Manual](https://www.renesas.com/en/document/mah/h8300h-series-software-manual).
- The older instruction-set reference retained for decoding H8 instructions
  and checking operand semantics: Hitachi document ADE-602-053A.
  [H8/300H Series Programming Manual, hosted by ManualsLib](https://www.manualslib.com/manual/1585252/Hitachi-H8-300h-Series.html).
- The accelerometer's sample representation, registers, measurement settings,
  and operating modes. The consulted edition is BST-BMA150-DS000-06,
  version 1.6, October 30, 2008.
  [Bosch BMA150 datasheet, hosted by DigiKey](https://media.digikey.com/pdf/Data%20Sheets/Bosch/BMA150.pdf).
- The serial EEPROM's commands, addressing, page programming, and timing
  constraints. The consulted family datasheet covers M95512-W, M95512-R,
  and M95512-DF: DS4192, Rev.24, September 2021.
  [ST M95512-W/-R/-DF datasheet](https://www.st.com/resource/en/datasheet/m95512-w.pdf).
- LCD commands, page and column addressing, display RAM, and initial-line
  selection. The consulted edition is V1.0, July 1, 2008. Dmitry's article
  records the August 2026 NT7508 identification and credits Lukas Unguraitis's
  research.
  [Novatek NT7508 datasheet, hosted by Orient Display](https://www.orientdisplay.com/wp-content/uploads/2022/08/NT7508_V1.0.pdf).
- The earlier LCD comparison reference for four-level grayscale display
  memory and command encodings: Rev.1.0, August 2002. The subsequent LCD
  investigation identified NT7508 as the better match for the firmware's
  command sequence.
  [Solomon Systech SSD1854 datasheet mirror](https://datasheet4u.com/pdf-down/S/S/D/SSD1854-SolomonSystech.pdf).

## Compiler documentation and C behavior

- C representation, calling conventions, compiler options, assembly,
  libraries, and linking. The retained manual is Compiler Package Ver.6.01,
  REJ10B0161-0100, Rev.1.00, January 12, 2005.
  [Renesas H8S/H8/300 compiler-package manual](https://www.renesas.com/en/document/mat/h8s-h8300-series-cc-compiler-assembler-optimizing-linkage-editor-compiler-package-v601-users-manual).
- HEW project and build behavior. Section 3.10 documents the default
  alphabetical object-link order used as context for this project's module
  naming and organization.
  [HEW V4.02 User's Manual](https://www.renesas.com/en/document/mat/high-performance-embedded-workshop-v402-users-manual).
- Compiler behavior, extensions, options, and examples relevant to the
  historical H8 toolchain.
  [Renesas H8S/H8/300 compiler application note](https://www.renesas.com/en/document/apn/h8s-h8300-series-cc-compiler-package-application-note).
- Background on structure padding, alignment, addressing, and optimization,
  consulted during the storage-reconstruction work: AN0403008, Rev.1.00,
  March 2004.
  [Renesas Embedded C Programming III — Optimization](https://www.renesas.com/en/document/apn/embedded-programming-iii-ecprogramiiiopt).
- C committee discussion of C90 union-member interpretation, consulted when
  reviewing the resident-resource byte view.
  [WG14 N847](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n847.htm).
- Configuring target integer widths for static analysis, used to establish
  the project's H8 platform configuration.
  [Cppcheck platform documentation](https://cppcheck.sourceforge.io/manual.html#platform).
- The relationship between compiler-package releases and their constituent
  compiler, assembler, linker, and library tools.
  [Renesas software component list](https://www.renesas.com/en/document/oth/cc-compiler-package-h8sx-h8s-h8-family-software-component-list).
- Official release information for the first supported compiler suite.
  [V6.02 Release 01 announcement](https://www.renesas.com/en/document/tnr/cc-compiler-package-h8sx-h8s-and-h8-mcu-families-revised-v602-release-01).
- Official release information for the second supported compiler suite.
  [V6.02 Release 02 announcement](https://www.renesas.com/en/document/tnr/cc-compiler-package-h8sx-h8s-and-h8-mcu-families-revised-v602-release-02).

## Reconstruction methods and tools

- The H8 instruction decoder used during reconstruction. The retained tool
  is GNU Binutils 2.16.1's `h8300-hms-objdump`.
  [GNU Binutils 2.16.1 release](https://ftp.gnu.org/gnu/binutils/binutils-2.16.1.tar.bz2).
- Background reading on inferring types from machine code, consulted during
  the storage-context investigation: Noonan, Loginov and Cok, 2016.
  [Polymorphic Type Inference for Machine Code](https://arxiv.org/abs/1603.05495v2).

## Artwork and setup tools

- A public implementation of the NCG container and tile representation used
  by the extracted sprite files.
  [NitroPaint's NCG reader](https://github.com/Garhoogin/NitroPaint/blob/be4d73c4843350665e0547590d7449996c6c33e8/NitroPaint/object/NitroCharacter.c#L642).
- A public implementation of the NCL palette container used for editing
  those sprites.
  [NitroPaint's NCL writer](https://github.com/Garhoogin/NitroPaint/blob/be4d73c4843350665e0547590d7449996c6c33e8/NitroPaint/object/NitroPalette.c#L513).
- Another public reader documenting the NCCG container through its
  implementation.
  [Tinke's NCCG reader](https://github.com/pleonex/tinke/blob/3263fc2baeea40c847e5267f826730210864fb8c/Plugins/Images/Images/NCCG.cs).
- The image library used for BMP artwork and font-sheet handling.
  [Pillow's BMP documentation](https://pillow.readthedocs.io/en/stable/handbook/image-file-formats.html#bmp).
- InstallShield extraction work that informed the project's installer parser
  and decoding approach.
  [ISx](https://github.com/lifenjoiner/ISx).
- Public InstallShield cabinet extraction code that also informed
  compiler-package extraction.
  [Unshield](https://github.com/twogood/unshield).
- Installer behavior and extraction options consulted while evaluating
  compiler-input handling.
  [Revenera's InstallShield command-line reference](https://docs.revenera.com/installshield/helplibrary/IHelpSetup_EXECmdLine.htm).
- The compatibility tool used to execute historical Windows compiler
  programs on supported Unix hosts.
  [Wibo](https://github.com/decompals/wibo).

## Build design

These projects and manuals informed contributor setup and build tooling.

- A decompilation project template consulted for Python configuration,
  Ninja integration, tool provisioning, and executable overrides.
  [DTK template, consulted revision](https://github.com/encounter/dtk-template/tree/95a941f755919ebe50c1725a4ce73524470e7a02).
- A working example of user-supplied original inputs, generated build
  configuration, and managed compiler wrappers.
  [The Wind Waker decompilation](https://github.com/zeldaret/tww/tree/ff768cc1d9ed10e884e006773a5f9ac6a8d6eff4).
- An additional comparison for contributor setup, historical compiler
  execution, and development environments.
  [Super Smash Bros. Melee decompilation](https://github.com/doldecomp/melee/tree/35acdca612f5dae409a37cb481fd3230cd42d560).
- A comparison for manually installed compiler wrappers and ordinary
  host prerequisites.
  [New Super Mario Bros. Wii decompilation](https://github.com/NSMBW-Community/NSMBW-Decomp/tree/5e9c76d50573d5659ba70bb174553e5d2ec3c708).
- A concrete example of extracting reusable artwork inputs before
  compilation, consulted when designing the separate artwork-extraction step.
  [Super Mario 64 asset extractor](https://github.com/n64decomp/sm64/blob/master/extract_assets.py).
- Another example of a separate asset-setup stage before normal builds.
  [Ocarina of Time setup documentation](https://github.com/zeldaret/oot#4-setup-the-rom-and-build-process).
- The reasoning behind generated build graphs and explicit dependencies.
  [Ninja's design documentation](https://ninja-build.org/manual.html#_philosophical_overview).
- Managed Python environments and dependency locking used by the project.
  [uv project documentation](https://docs.astral.sh/uv/guides/projects/).

## Historical development context

The coding references informed this project's choice of conventions. The
Pokéwalker team's original house style remains unknown.

- Contemporary Japanese embedded-development guidance consulted when
  choosing consistent naming, type, and coding policies.
  [IPA/SEC embedded coding guide](https://www.ipa.go.jp/archive/publish/qv6pgp0000000x2q-att/000005106.pdf).
- A contemporary Renesas firmware example providing context for period
  C conventions.
  [Renesas user-boot application note](https://www.renesas.com/en/document/apn/example-using-user-boot-mode-renesas-018um-flash-devices-xmodem-data-transfer).
- Another contemporary H8 firmware example consulted during the style
  investigation. This example uses the IAR compiler and project generator.
  [Renesas H8/38024F peripheral examples](https://www.renesas.com/en/document/apn/example-code-h838024f-peripherals).
- Nintendo's public account of the Pokéwalker's development and intended
  experience.
  [Iwata Asks: HeartGold and SoulSilver, section 5](https://www.nintendo.co.jp/ds/interview/ipkj/vol1/index5.html).

## Related projects

Community projects for Pokéwalker emulation, hardware recreation, artwork,
and data inspection.

- A community implementation connecting infrared communication, EEPROM
  records, audio, and application behavior.
  [PicoWalker core](https://github.com/mamba2410/picowalker-core).
- mamba2410's recreation of the Pokéwalker using the Raspberry Pi RP2350
  microcontroller. This repository supplies the device drivers and board
  integration for the application core.
  [PicoWalker](https://github.com/mamba2410/picowalker).
- PCB designs for the PicoWalker recreation, including RP2350 hardware and
  a Raspberry Pi Pico 2 carrier board.
  [PicoWalker hardware](https://github.com/mamba2410/picowalker-hardware).
- jpcerrone's experimental Windows emulator for exploring the Pokéwalker's
  menus and minigames with ROM and EEPROM images.
  [PokeStroller](https://github.com/jpcerrone/pokestroller).
- UnrealPowerz' prototype Pokéwalker emulator, including CPU, EEPROM, LCD,
  and button emulation.
  [Powar](https://github.com/UnrealPowerz/powar).
- UnrealPowerz' browser application for viewing and editing data and artwork
  in Pokéwalker EEPROM images.
  [Pokéwalker EEPROM Editor](https://unrealpowerz.github.io/pokewalker-eeprom-editor/).
  Its implementation provides readable views of EEPROM contents for format
  exploration.
  [EEPROM editor source](https://github.com/UnrealPowerz/pokewalker-eeprom-editor).
- PoroCYon's project for dumping Pokéwalker ROM and EEPROM contents.
  [Pokéwalker ROM dumper](https://gitlab.ulyssis.org/pcy/pokewalker-rom-dumper).
- mamba2410's image viewer and converter, which displays packed Pokéwalker
  images in a terminal and converts grayscale BMP artwork to the device's
  image format.
  [pw-lcd](https://github.com/mamba2410/pw-lcd).
- An interoperability project illustrating how HGSS's Pokéwalker protocol
  can operate through another infrared transport.
  [RtcPwalker](https://github.com/francesco265/RtcPwalker).
