"""Read compiler files from InstallShield updater executables.

The container layout and block decoder follow ISx and Unshield; their notices
are in docs/third-party.md. Extraction only reads the supplied executable.
"""

from __future__ import annotations

import hashlib
import struct
import zlib
from dataclasses import dataclass
from pathlib import PurePosixPath

from .common import InputError

MAX_PACKAGE_BYTES = 512 * 1024 * 1024
MAX_FILE_BYTES = 128 * 1024 * 1024
MAX_EXPANDED_BYTES = 512 * 1024 * 1024
MAX_MEMBERS = 50_000
WINDOWS_DEVICES = {
    "con",
    "prn",
    "aux",
    "nul",
    *(f"com{i}" for i in range(1, 10)),
    *(f"lpt{i}" for i in range(1, 10)),
}


def span(data: bytes, offset: int, size: int) -> bytes:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise InputError("Truncated InstallShield input; a declared range exceeds the file.")
    return data[offset : offset + size]


def u16(data: bytes, offset: int) -> int:
    return struct.unpack("<H", span(data, offset, 2))[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack("<I", span(data, offset, 4))[0]


def string_at(data: bytes, offset: int) -> tuple[bytes, int]:
    span(data, offset, 1)
    end = data.find(b"\0", offset, min(len(data), offset + 4096))
    if end < 0:
        raise InputError("InstallShield input contains an unterminated filename or field.")
    return data[offset:end], end + 1


def relative_path(name: str) -> str:
    """Keep archive paths portable when selecting or writing their members."""
    normalized = name.replace("\\", "/")
    path = PurePosixPath(normalized)
    if (
        not normalized
        or path.is_absolute()
        or any(
            part in {"", ".", ".."}
            or part.endswith((".", " "))
            or part.split(".")[0].casefold() in WINDOWS_DEVICES
            or any(char in part for char in ':<>"|?*')
            or any(ord(char) < 32 for char in part)
            for part in normalized.split("/")
        )
    ):
        raise InputError(f"Unsupported archive path: {name!r}.")
    return path.as_posix()


def installer_members(data: bytes) -> dict[str, bytes]:
    """Find the cabinet files appended to an InstallShield launcher."""
    if len(data) > MAX_PACKAGE_BYTES or data[:2] != b"MZ":
        raise InputError("Input is not a supported InstallShield updater executable.")
    pe = u32(data, 0x3C)
    if span(data, pe, 4) != b"PE\0\0":
        raise InputError("Invalid updater executable header.")
    section_count = u16(data, pe + 6)
    if not 1 <= section_count <= 96:
        raise InputError("Invalid updater executable section count.")
    table = pe + 24 + u16(data, pe + 20)
    span(data, table, 40 * section_count)
    offset = max(
        u32(data, table + 40 * index + 16) + u32(data, table + 40 * index + 20)
        for index in range(section_count)
    )
    span(data, offset, 0)
    required = {"data1.cab", "data1.hdr", "data2.cab"}
    result = {}
    for _ in range(MAX_MEMBERS):
        if offset == len(data):
            break
        fields = []
        for _ in range(4):
            field, offset = string_at(data, offset)
            fields.append(field)
        if not fields[3].isdigit() or len(fields[3]) > 12:
            raise InputError("Invalid updater member size.")
        size = int(fields[3])
        member = span(data, offset, size)
        offset += size
        try:
            name = fields[0].decode("ascii").casefold()
        except UnicodeDecodeError:
            continue
        if name not in required:
            continue
        if name in result:
            raise InputError("Duplicate updater cabinet.")
        result[name] = member
        if required <= result.keys():
            return result
    raise InputError(
        "Updater is missing its cabinets. Import an installed compiler directory instead."
    )


def decompress_blocks(data: bytes, expanded_size: int) -> bytes:
    if not 0 <= expanded_size <= MAX_FILE_BYTES:
        raise InputError("Compiler file exceeds the extraction size limit.")
    output = bytearray()
    offset = 0
    while offset < len(data):
        size = u16(data, offset)
        offset += 2
        if size == 0:
            raise InputError("Invalid compressed block size.")
        stored = span(data, offset, size)
        offset += size
        decoder = zlib.decompressobj(-15)
        try:
            block = decoder.decompress(stored + b"\0", expanded_size - len(output) + 1)
        except zlib.error as error:
            raise InputError("Invalid compressed InstallShield block.") from error
        if not decoder.eof or len(output) + len(block) > expanded_size:
            raise InputError("Compressed block exceeds its declared size.")
        output.extend(block)
    if len(output) != expanded_size:
        raise InputError("Decompressed file has the wrong size.")
    return bytes(output)


@dataclass(frozen=True)
class Member:
    name: str
    flags: int
    expanded_size: int
    stored_size: int
    offset: int
    volume: int
    checksum: bytes


def cabinet_files(header: bytes) -> list[Member]:
    if header[:4] != b"ISc(":
        raise InputError("Invalid InstallShield cabinet header.")
    base = u32(header, 12)
    strings = base + u32(header, base + 12)
    count = u32(header, base + 40)
    records = strings + u32(header, base + 44)
    if count > MAX_MEMBERS:
        raise InputError("InstallShield input contains too many files.")
    span(header, records, count * 87)
    result = []
    for index in range(count):
        descriptor = records + index * 87
        flags = u16(header, descriptor)
        expanded, stored, offset = struct.unpack("<QQQ", span(header, descriptor + 2, 24))
        if flags & 8 or not offset:
            continue
        name, _ = string_at(header, strings + u32(header, descriptor + 58))
        directory_index = u16(header, descriptor + 62)
        directory, _ = string_at(header, strings + u32(header, strings + directory_index * 4))
        try:
            directory_name = directory.decode("ascii").replace("\\", "/")
            filename = name.decode("ascii")
        except UnicodeDecodeError:
            continue
        if not {"bin", "include"}.intersection(directory_name.casefold().split("/")):
            continue
        path = relative_path(directory_name + "/" + filename)
        result.append(
            Member(
                path,
                flags,
                expanded,
                stored,
                offset,
                u16(header, descriptor + 85),
                span(header, descriptor + 26, 16),
            )
        )
    return result


def suite_path(name: str) -> tuple[str, str] | None:
    """Split the common suite root from its bin/include member path."""
    parts = name.split("/")
    for index, part in enumerate(parts[:-1]):
        if part.casefold() in {"bin", "include"}:
            return "/".join(parts[:index]).casefold(), "/".join(parts[index:])
    return None


def extract_member(member: Member, cabinets: dict[str, bytes]) -> bytes:
    if member.flags & 3:
        raise InputError(
            "Unsupported cabinet encoding. Import an installed compiler directory instead."
        )
    cabinet = cabinets.get(f"data{member.volume}.cab")
    if cabinet is None:
        raise InputError("Compiler file refers to a missing cabinet volume.")
    stored = span(cabinet, member.offset, member.stored_size)
    decoded = decompress_blocks(stored, member.expanded_size) if member.flags & 4 else stored
    if len(decoded) != member.expanded_size:
        raise InputError("Uncompressed compiler file has the wrong size.")
    if hashlib.md5(decoded, usedforsecurity=False).digest() != member.checksum:
        raise InputError(f"Cabinet checksum failed for {member.name}; the package may be damaged.")
    return decoded


def compiler_members(data: bytes, catalog: dict) -> dict[str, bytes]:
    cabinets = installer_members(data)
    members = cabinet_files(cabinets["data1.hdr"])
    roots = {
        suite_path(member.name)[0]
        for member in members
        if member.name.casefold().endswith("/bin/ch38.exe")
    }
    if len(roots) != 1:
        raise InputError(
            "Updater must contain one compiler suite. Import its installed bin/include directory."
        )
    root = roots.pop()
    preferred = {}
    canonical = {}
    for suite in catalog.values():
        for name, checksum in suite["files"].items():
            preferred.setdefault(name.casefold(), set()).add(checksum)
            canonical[name.casefold()] = name
    variants = {}
    expanded_total = 0
    for member in members:
        group, name = suite_path(member.name)
        if group != root:
            continue
        expanded_total += member.expanded_size
        if expanded_total > MAX_EXPANDED_BYTES or member.expanded_size > MAX_FILE_BYTES:
            raise InputError("Compiler files exceed the extraction size limit.")
        decoded = extract_member(member, cabinets)
        checksum = hashlib.sha256(decoded).hexdigest()
        variants.setdefault(name.casefold(), {})[checksum] = decoded
        canonical.setdefault(name.casefold(), name)
    output = {}
    for name, copies in variants.items():
        # Some updaters offer alternative linkers at the same installed path.
        known = copies.keys() & preferred.get(name, set())
        if len(copies) > 1 and len(known) != 1:
            raise InputError(
                f"Updater contains conflicting copies of {name}. "
                "Import an installed compiler directory to select one complete suite."
            )
        checksum = next(iter(known if known else copies))
        output[canonical[name]] = copies[checksum]
    return output
