#!/usr/bin/env python3
"""Deterministic reference-artifact tooling for RR2NW.

The tool intentionally uses only the Python standard library so it can run on
a clean Windows build agent. Generated reports contain logical labels and
relative paths, never the absolute source path supplied on the command line.
"""

from __future__ import annotations

import argparse
import datetime as dt
import fnmatch
import hashlib
import json
import os
import re
import struct
import sys
import tempfile
from pathlib import Path, PurePosixPath
from typing import Any, Dict, Iterable, List, Mapping, Optional, Sequence, Tuple


MANIFEST_SCHEMA = "rr2nw.file-manifest/v1"
DIFF_SCHEMA = "rr2nw.manifest-diff/v1"
PE_SCHEMA = "rr2nw.pe-report/v1"
FILES_SCHEMA = "rr2nw.file-artifacts/v1"
M0_SUMMARY_SCHEMA = "rr2nw.m0-summary/v1"
PARITY_LEDGER_SCHEMA = "rr2nw.parity-ledger/v1"
HASH_CHUNK_SIZE = 1024 * 1024
DEFAULT_TEXT_EXTENSIONS = frozenset(
    {
        ".bat",
        ".c",
        ".cc",
        ".cfg",
        ".cpp",
        ".h",
        ".hpp",
        ".inc",
        ".ini",
        ".mak",
        ".make",
        ".rc",
        ".rt",
        ".sc",
        ".sci",
        ".txt",
    }
)


class ReferenceError(RuntimeError):
    """An expected validation or artifact-format failure."""


def sha256_file(path: Path) -> Tuple[str, os.stat_result]:
    before = path.stat()
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(HASH_CHUNK_SIZE)
            if not block:
                break
            digest.update(block)
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ReferenceError(f"file changed while hashing: {path}")
    return digest.hexdigest().upper(), after


def normalized_text_sha256(path: Path) -> str:
    data = path.read_bytes()
    data = data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    data = re.sub(rb"[ \t]+(?=\n|$)", b"", data)
    return hashlib.sha256(data).hexdigest().upper()


def format_mtime_utc(stat_result: os.stat_result) -> str:
    value = dt.datetime.fromtimestamp(stat_result.st_mtime, tz=dt.timezone.utc)
    return value.isoformat(timespec="microseconds").replace("+00:00", "Z")


def parse_text_extensions(value: str) -> frozenset[str]:
    extensions = set()
    for raw_extension in value.split(","):
        extension = raw_extension.strip().lower()
        if not extension:
            continue
        if not extension.startswith("."):
            extension = "." + extension
        extensions.add(extension)
    return frozenset(extensions)


def is_link_or_reparse(path: Path) -> bool:
    if path.is_symlink():
        return True
    try:
        attributes = path.lstat().st_file_attributes
    except AttributeError:
        return False
    reparse_flag = getattr(__import__("stat"), "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    return bool(reparse_flag and attributes & reparse_flag)


def matches_any(path: str, patterns: Sequence[str]) -> bool:
    return any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns)


def iter_regular_files(root: Path, exclude_patterns: Sequence[str]) -> Iterable[Tuple[str, Path]]:
    for directory, directory_names, file_names in os.walk(root, followlinks=False):
        directory_path = Path(directory)
        directory_names.sort(key=lambda item: (item.casefold(), item))
        file_names.sort(key=lambda item: (item.casefold(), item))

        retained_directories = []
        for directory_name in directory_names:
            candidate = directory_path / directory_name
            relative = candidate.relative_to(root).as_posix()
            if matches_any(relative + "/", exclude_patterns):
                continue
            if is_link_or_reparse(candidate):
                raise ReferenceError(f"links/reparse points are not allowed: {relative}")
            retained_directories.append(directory_name)
        directory_names[:] = retained_directories

        for file_name in file_names:
            candidate = directory_path / file_name
            relative = candidate.relative_to(root).as_posix()
            if matches_any(relative, exclude_patterns):
                continue
            if is_link_or_reparse(candidate):
                raise ReferenceError(f"links/reparse points are not allowed: {relative}")
            if not candidate.is_file():
                raise ReferenceError(f"non-regular filesystem entry: {relative}")
            yield relative, candidate


def build_manifest(
    root: Path,
    label: str,
    text_extensions: frozenset[str],
    exclude_patterns: Sequence[str],
) -> Dict[str, Any]:
    root = root.resolve()
    if not root.is_dir():
        raise ReferenceError(f"manifest root is not a directory: {root}")

    files: List[Dict[str, Any]] = []
    path_keys: Dict[str, str] = {}
    total_bytes = 0
    for relative, path in iter_regular_files(root, exclude_patterns):
        folded = relative.casefold()
        previous = path_keys.get(folded)
        if previous is not None:
            raise ReferenceError(
                "case-insensitive path collision: " f"{previous!r} and {relative!r}"
            )
        path_keys[folded] = relative

        sha256, stat_result = sha256_file(path)
        entry: Dict[str, Any] = {
            "mtime_ns": stat_result.st_mtime_ns,
            "mtime_utc": format_mtime_utc(stat_result),
            "path": relative,
            "sha256": sha256,
            "size": stat_result.st_size,
        }
        if PurePosixPath(relative).suffix.lower() in text_extensions:
            entry["normalized_text_sha256"] = normalized_text_sha256(path)
        files.append(entry)
        total_bytes += stat_result.st_size

    files.sort(key=lambda item: (item["path"].casefold(), item["path"]))
    return {
        "algorithm": "SHA-256",
        "case_policy": "preserve-and-reject-casefold-collisions",
        "excluded_patterns": list(exclude_patterns),
        "file_count": len(files),
        "files": files,
        "normalization": {
            "algorithm": "CRLF/CR-to-LF-and-strip-trailing-ASCII-space-tab",
            "text_extensions": sorted(text_extensions),
        },
        "root_label": label,
        "schema": MANIFEST_SCHEMA,
        "total_bytes": total_bytes,
    }


def load_json(path: Path) -> Dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as stream:
            value = json.load(stream)
    except (OSError, json.JSONDecodeError) as error:
        raise ReferenceError(f"cannot read JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise ReferenceError(f"JSON root must be an object: {path}")
    return value


def write_json(path: Path, value: Mapping[str, Any]) -> None:
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    temporary_name: Optional[str] = None
    try:
        with tempfile.NamedTemporaryFile(
            "w", encoding="utf-8", newline="\n", dir=path.parent, delete=False
        ) as stream:
            temporary_name = stream.name
            stream.write(payload)
        os.replace(temporary_name, path)
    finally:
        if temporary_name and os.path.exists(temporary_name):
            os.unlink(temporary_name)


def validate_manifest(value: Mapping[str, Any], description: str) -> None:
    if value.get("schema") != MANIFEST_SCHEMA:
        raise ReferenceError(f"{description} is not a {MANIFEST_SCHEMA} manifest")
    if not isinstance(value.get("files"), list):
        raise ReferenceError(f"{description} has no files array")


def extension_for(path: str) -> str:
    return PurePosixPath(path).suffix.upper() or "<NONE>"


def add_extension_stat(stats: Dict[str, Dict[str, int]], extension: str, status: str) -> None:
    entry = stats.setdefault(
        extension,
        {
            "changed": 0,
            "common": 0,
            "exact": 0,
            "left_only": 0,
            "normalized_equal": 0,
            "right_only": 0,
        },
    )
    entry[status] += 1
    if status in {"exact", "normalized_equal", "changed"}:
        entry["common"] += 1


def build_manifest_diff(left: Mapping[str, Any], right: Mapping[str, Any]) -> Dict[str, Any]:
    validate_manifest(left, "left input")
    validate_manifest(right, "right input")
    left_by_key = {entry["path"].casefold(): entry for entry in left["files"]}
    right_by_key = {entry["path"].casefold(): entry for entry in right["files"]}
    if len(left_by_key) != len(left["files"]) or len(right_by_key) != len(right["files"]):
        raise ReferenceError("input manifest contains case-insensitive path collisions")

    records: List[Dict[str, Any]] = []
    counts = {
        "case_changed": 0,
        "changed": 0,
        "common": 0,
        "exact": 0,
        "left_only": 0,
        "normalized_equal": 0,
        "right_only": 0,
    }
    extension_stats: Dict[str, Dict[str, int]] = {}

    for key in sorted(set(left_by_key) | set(right_by_key)):
        left_entry = left_by_key.get(key)
        right_entry = right_by_key.get(key)
        if left_entry is None:
            status = "right_only"
            record = {"right": right_entry, "status": status}
            extension = extension_for(right_entry["path"])
        elif right_entry is None:
            status = "left_only"
            record = {"left": left_entry, "status": status}
            extension = extension_for(left_entry["path"])
        else:
            counts["common"] += 1
            case_changed = left_entry["path"] != right_entry["path"]
            if case_changed:
                counts["case_changed"] += 1
            if left_entry["sha256"] == right_entry["sha256"]:
                status = "exact"
            elif (
                left_entry.get("normalized_text_sha256")
                and right_entry.get("normalized_text_sha256")
                and left_entry["normalized_text_sha256"]
                == right_entry["normalized_text_sha256"]
            ):
                status = "normalized_equal"
            else:
                status = "changed"
            record = {
                "case_changed": case_changed,
                "left": left_entry,
                "right": right_entry,
                "status": status,
            }
            extension = extension_for(right_entry["path"])
        counts[status] += 1
        add_extension_stat(extension_stats, extension, status)
        records.append(record)

    return {
        "extension_stats": dict(sorted(extension_stats.items())),
        "left": {
            "file_count": left["file_count"],
            "root_label": left["root_label"],
            "total_bytes": left["total_bytes"],
        },
        "records": records,
        "right": {
            "file_count": right["file_count"],
            "root_label": right["root_label"],
            "total_bytes": right["total_bytes"],
        },
        "schema": DIFF_SCHEMA,
        "summary": counts,
    }


def build_parity_ledger(diff_report: Mapping[str, Any]) -> Dict[str, Any]:
    if diff_report.get("schema") != DIFF_SCHEMA:
        raise ReferenceError(f"parity input is not a {DIFF_SCHEMA} report")
    entries: List[Dict[str, Any]] = []
    id_to_path: Dict[str, str] = {}
    for record in diff_report["records"]:
        if record["status"] == "exact":
            continue
        left = record.get("left")
        right = record.get("right")
        path = (right or left)["path"]
        identity_path = path.casefold()
        stable_id = "RP-" + hashlib.sha256(identity_path.encode("utf-8")).hexdigest()[:12].upper()
        previous_path = id_to_path.get(stable_id)
        if previous_path is not None and previous_path != identity_path:
            raise ReferenceError(
                f"parity ID collision for {previous_path!r} and {identity_path!r}"
            )
        id_to_path[stable_id] = identity_path
        entry: Dict[str, Any] = {
            "classification": "UNKNOWN",
            "diff_status": record["status"],
            "extension": extension_for(path),
            "id": stable_id,
            "path": path,
        }
        if left is not None:
            entry["source_sha256"] = left["sha256"]
        if right is not None:
            entry["retail_sha256"] = right["sha256"]
        if record.get("case_changed"):
            entry["source_path"] = left["path"]
            entry["retail_path"] = right["path"]
        entries.append(entry)
    entries.sort(key=lambda item: (item["path"].casefold(), item["path"]))
    return {
        "classification_counts": {"UNKNOWN": len(entries)},
        "entries": entries,
        "entry_count": len(entries),
        "left_label": diff_report["left"]["root_label"],
        "right_label": diff_report["right"]["root_label"],
        "schema": PARITY_LEDGER_SCHEMA,
    }


def checked_unpack(data: bytes, offset: int, fmt: str) -> Tuple[Any, ...]:
    size = struct.calcsize(fmt)
    if offset < 0 or offset + size > len(data):
        raise ReferenceError(f"PE structure extends beyond file at 0x{offset:X}")
    return struct.unpack_from(fmt, data, offset)


def read_ascii_z(data: bytes, offset: int, limit: int = 4096) -> str:
    if offset < 0 or offset >= len(data):
        raise ReferenceError(f"PE string offset outside file: 0x{offset:X}")
    end = data.find(b"\0", offset, min(len(data), offset + limit))
    if end < 0:
        raise ReferenceError(f"unterminated PE string at file offset 0x{offset:X}")
    return data[offset:end].decode("ascii", errors="replace")


MACHINE_NAMES = {0x014C: "I386", 0x8664: "AMD64", 0x01C4: "ARMNT", 0xAA64: "ARM64"}
SUBSYSTEM_NAMES = {
    1: "NATIVE",
    2: "WINDOWS_GUI",
    3: "WINDOWS_CUI",
    7: "POSIX_CUI",
    9: "WINDOWS_CE_GUI",
    10: "EFI_APPLICATION",
}
SECTION_FLAGS = {
    0x00000020: "CODE",
    0x00000040: "INITIALIZED_DATA",
    0x00000080: "UNINITIALIZED_DATA",
    0x20000000: "EXECUTE",
    0x40000000: "READ",
    0x80000000: "WRITE",
}
DLL_FLAGS = {
    0x0020: "HIGH_ENTROPY_VA",
    0x0040: "DYNAMIC_BASE",
    0x0080: "FORCE_INTEGRITY",
    0x0100: "NX_COMPAT",
    0x0400: "NO_SEH",
    0x0800: "NO_BIND",
    0x1000: "APPCONTAINER",
    0x2000: "WDM_DRIVER",
    0x4000: "GUARD_CF",
    0x8000: "TERMINAL_SERVER_AWARE",
}


def decode_flags(value: int, known: Mapping[int, str]) -> List[str]:
    return [name for flag, name in known.items() if value & flag]


def timestamp_report(timestamp: int) -> Optional[str]:
    if timestamp == 0:
        return None
    try:
        value = dt.datetime.fromtimestamp(timestamp, tz=dt.timezone.utc)
    except (OverflowError, OSError, ValueError):
        return None
    return value.isoformat(timespec="seconds").replace("+00:00", "Z")


def find_rva_section(sections: Sequence[Mapping[str, Any]], rva: int) -> Optional[Mapping[str, Any]]:
    for section in sections:
        span = max(section["virtual_size"], section["raw_size"])
        if section["virtual_address"] <= rva < section["virtual_address"] + span:
            return section
    return None


def rva_to_offset(
    sections: Sequence[Mapping[str, Any]], rva: int, size_of_headers: int, data_size: int
) -> int:
    if 0 <= rva < size_of_headers and rva < data_size:
        return rva
    section = find_rva_section(sections, rva)
    if section is None:
        raise ReferenceError(f"RVA 0x{rva:X} does not belong to a PE section")
    delta = rva - section["virtual_address"]
    if delta >= section["raw_size"]:
        raise ReferenceError(f"RVA 0x{rva:X} has no bytes in the PE file")
    offset = section["raw_pointer"] + delta
    if offset >= data_size:
        raise ReferenceError(f"RVA 0x{rva:X} maps beyond the PE file")
    return offset


def parse_imports(
    data: bytes,
    sections: Sequence[Mapping[str, Any]],
    size_of_headers: int,
    import_rva: int,
    pe32_plus: bool,
) -> List[Dict[str, Any]]:
    if import_rva == 0:
        return []
    descriptor_offset = rva_to_offset(sections, import_rva, size_of_headers, len(data))
    imports: List[Dict[str, Any]] = []
    thunk_format = "<Q" if pe32_plus else "<I"
    thunk_size = 8 if pe32_plus else 4
    ordinal_mask = 1 << (63 if pe32_plus else 31)
    address_mask = ordinal_mask - 1

    for descriptor_index in range(1024):
        offset = descriptor_offset + descriptor_index * 20
        descriptor = checked_unpack(data, offset, "<IIIII")
        if not any(descriptor):
            break
        original_thunk, _, _, name_rva, first_thunk = descriptor
        dll_name = read_ascii_z(
            data, rva_to_offset(sections, name_rva, size_of_headers, len(data))
        )
        lookup_rva = original_thunk or first_thunk
        symbols: List[Dict[str, Any]] = []
        if lookup_rva:
            thunk_offset = rva_to_offset(sections, lookup_rva, size_of_headers, len(data))
            for thunk_index in range(65536):
                value = checked_unpack(data, thunk_offset + thunk_index * thunk_size, thunk_format)[0]
                if value == 0:
                    break
                if value & ordinal_mask:
                    symbols.append({"ordinal": value & 0xFFFF})
                else:
                    name_offset = rva_to_offset(
                        sections, value & address_mask, size_of_headers, len(data)
                    )
                    hint = checked_unpack(data, name_offset, "<H")[0]
                    symbols.append({"hint": hint, "name": read_ascii_z(data, name_offset + 2)})
            else:
                raise ReferenceError(f"import thunk limit exceeded for {dll_name}")
        imports.append({"dll": dll_name, "symbols": symbols})
    else:
        raise ReferenceError("PE import descriptor limit exceeded")
    return imports


def parse_rva(value: str) -> int:
    text = value.strip().lower()
    base = 16 if text.startswith("0x") or any(char in "abcdef" for char in text) else 10
    return int(text, base)


def inspect_pe(path: Path, probe_rvas: Sequence[int]) -> Dict[str, Any]:
    path = path.resolve()
    sha256, stat_result = sha256_file(path)
    data = path.read_bytes()
    if len(data) < 64 or data[:2] != b"MZ":
        raise ReferenceError(f"not an MZ executable: {path.name}")
    pe_offset = checked_unpack(data, 0x3C, "<I")[0]
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise ReferenceError(f"missing PE signature: {path.name}")

    coff_offset = pe_offset + 4
    machine, section_count, timestamp, _, _, optional_size, characteristics = checked_unpack(
        data, coff_offset, "<HHIIIHH"
    )
    optional_offset = coff_offset + 20
    magic = checked_unpack(data, optional_offset, "<H")[0]
    if magic == 0x10B:
        pe32_plus = False
        image_base = checked_unpack(data, optional_offset + 28, "<I")[0]
        directory_count_offset = optional_offset + 92
        directories_offset = optional_offset + 96
    elif magic == 0x20B:
        pe32_plus = True
        image_base = checked_unpack(data, optional_offset + 24, "<Q")[0]
        directory_count_offset = optional_offset + 108
        directories_offset = optional_offset + 112
    else:
        raise ReferenceError(f"unsupported PE optional-header magic 0x{magic:X}")

    entry_point = checked_unpack(data, optional_offset + 16, "<I")[0]
    section_alignment, file_alignment = checked_unpack(data, optional_offset + 32, "<II")
    size_of_image, size_of_headers, checksum = checked_unpack(
        data, optional_offset + 56, "<III"
    )
    subsystem, dll_characteristics = checked_unpack(data, optional_offset + 68, "<HH")
    directory_count = checked_unpack(data, directory_count_offset, "<I")[0]
    import_rva = 0
    import_size = 0
    if directory_count > 1:
        import_rva, import_size = checked_unpack(data, directories_offset + 8, "<II")

    section_table_offset = optional_offset + optional_size
    sections: List[Dict[str, Any]] = []
    for index in range(section_count):
        offset = section_table_offset + index * 40
        raw_name = checked_unpack(data, offset, "<8s")[0]
        name = raw_name.split(b"\0", 1)[0].decode("ascii", errors="replace")
        virtual_size, virtual_address, raw_size, raw_pointer = checked_unpack(
            data, offset + 8, "<IIII"
        )
        section_characteristics = checked_unpack(data, offset + 36, "<I")[0]
        sections.append(
            {
                "characteristics": f"0x{section_characteristics:08X}",
                "flags": decode_flags(section_characteristics, SECTION_FLAGS),
                "index": index,
                "name": name,
                "raw_pointer": raw_pointer,
                "raw_size": raw_size,
                "virtual_address": virtual_address,
                "virtual_size": virtual_size,
            }
        )

    warnings: List[str] = []
    try:
        imports = parse_imports(data, sections, size_of_headers, import_rva, pe32_plus)
    except ReferenceError as error:
        imports = []
        warnings.append(f"imports: {error}")

    probes: List[Dict[str, Any]] = []
    for rva in probe_rvas:
        section = find_rva_section(sections, rva)
        probe: Dict[str, Any] = {"rva": f"0x{rva:08X}"}
        if section is None:
            probe.update({"executable": False, "section": None})
        else:
            probe.update(
                {
                    "executable": "EXECUTE" in section["flags"],
                    "section": section["name"],
                    "section_flags": section["flags"],
                }
            )
            try:
                probe["file_offset"] = rva_to_offset(
                    sections, rva, size_of_headers, len(data)
                )
            except ReferenceError as error:
                probe["mapping_warning"] = str(error)
        probes.append(probe)

    entry_section = find_rva_section(sections, entry_point)
    return {
        "coff": {
            "characteristics": f"0x{characteristics:04X}",
            "linker_timestamp": timestamp,
            "linker_timestamp_utc": timestamp_report(timestamp),
            "machine": f"0x{machine:04X}",
            "machine_name": MACHINE_NAMES.get(machine, "UNKNOWN"),
            "section_count": section_count,
        },
        "file": {
            "mtime_ns": stat_result.st_mtime_ns,
            "mtime_utc": format_mtime_utc(stat_result),
            "name": path.name,
            "sha256": sha256,
            "size": stat_result.st_size,
        },
        "imports": imports,
        "optional_header": {
            "checksum": f"0x{checksum:08X}",
            "dll_characteristics": f"0x{dll_characteristics:04X}",
            "dll_flags": decode_flags(dll_characteristics, DLL_FLAGS),
            "entry_point_rva": f"0x{entry_point:08X}",
            "entry_point_section": entry_section["name"] if entry_section else None,
            "file_alignment": file_alignment,
            "image_base": f"0x{image_base:X}",
            "import_directory_rva": f"0x{import_rva:08X}",
            "import_directory_size": import_size,
            "magic": "PE32+" if pe32_plus else "PE32",
            "section_alignment": section_alignment,
            "size_of_headers": size_of_headers,
            "size_of_image": size_of_image,
            "subsystem": subsystem,
            "subsystem_name": SUBSYSTEM_NAMES.get(subsystem, "UNKNOWN"),
        },
        "probes": probes,
        "schema": PE_SCHEMA,
        "sections": sections,
        "warnings": warnings,
    }


def build_file_artifacts(paths: Sequence[Path], label: str) -> Dict[str, Any]:
    entries = []
    for path in sorted((item.resolve() for item in paths), key=lambda item: item.name.casefold()):
        if not path.is_file():
            raise ReferenceError(f"artifact is not a file: {path}")
        sha256, stat_result = sha256_file(path)
        entries.append(
            {
                "mtime_ns": stat_result.st_mtime_ns,
                "mtime_utc": format_mtime_utc(stat_result),
                "name": path.name,
                "sha256": sha256,
                "size": stat_result.st_size,
            }
        )
    return {"artifacts": entries, "label": label, "schema": FILES_SCHEMA}


def require_report(root: Path, name: str, schema: str) -> Tuple[Path, Dict[str, Any]]:
    path = root / name
    value = load_json(path)
    if value.get("schema") != schema:
        raise ReferenceError(f"{name} is not a {schema} report")
    return path, value


def summarized_pe(report: Mapping[str, Any]) -> Dict[str, Any]:
    imported_dlls: Dict[str, Dict[str, Any]] = {}
    for imported in report.get("imports", []):
        key = imported["dll"].casefold()
        entry = imported_dlls.setdefault(
            key, {"name": imported["dll"], "symbol_count": 0}
        )
        entry["symbol_count"] += len(imported.get("symbols", []))
    file_report = report["file"]
    return {
        "coff": report["coff"],
        "file": {
            "name": file_report["name"],
            "sha256": file_report["sha256"],
            "size": file_report["size"],
        },
        "imported_dlls": [imported_dlls[key] for key in sorted(imported_dlls)],
        "optional_header": report["optional_header"],
        "probes": report.get("probes", []),
        "sections": report["sections"],
        "warnings": report.get("warnings", []),
    }


def build_m0_summary(input_root: Path) -> Dict[str, Any]:
    input_root = input_root.resolve()
    source_path, source = require_report(
        input_root, "source-output.manifest.json", MANIFEST_SCHEMA
    )
    retail_path, retail = require_report(input_root, "retail.manifest.json", MANIFEST_SCHEMA)
    diff_path, source_diff = require_report(
        input_root, "source-to-retail.diff.json", DIFF_SCHEMA
    )
    _, retail_pe = require_report(input_root, "retail-nw.pe.json", PE_SCHEMA)

    summary: Dict[str, Any] = {
        "privacy": {
            "absolute_paths_included": False,
            "full_file_lists_included": False,
            "retail_payload_included": False,
        },
        "retail": {
            "file_count": retail["file_count"],
            "manifest_sha256": sha256_file(retail_path)[0],
            "root_label": retail["root_label"],
            "total_bytes": retail["total_bytes"],
        },
        "retail_pe": summarized_pe(retail_pe),
        "schema": M0_SUMMARY_SCHEMA,
        "source": {
            "file_count": source["file_count"],
            "manifest_sha256": sha256_file(source_path)[0],
            "root_label": source["root_label"],
            "total_bytes": source["total_bytes"],
        },
        "source_to_retail": {
            "diff_sha256": sha256_file(diff_path)[0],
            "extension_stats": source_diff["extension_stats"],
            "summary": source_diff["summary"],
        },
    }

    source_tree_path = input_root / "source-tree.manifest.json"
    if source_tree_path.is_file():
        source_tree = load_json(source_tree_path)
        validate_manifest(source_tree, "source tree input")
        summary["source_tree"] = {
            "file_count": source_tree["file_count"],
            "manifest_sha256": sha256_file(source_tree_path)[0],
            "root_label": source_tree["root_label"],
            "total_bytes": source_tree["total_bytes"],
        }

    disc_path = input_root / "retail-disc.manifest.json"
    if disc_path.is_file():
        disc = load_json(disc_path)
        validate_manifest(disc, "retail disc input")
        summary["retail_disc"] = {
            "file_count": disc["file_count"],
            "manifest_sha256": sha256_file(disc_path)[0],
            "root_label": disc["root_label"],
            "total_bytes": disc["total_bytes"],
        }

    ledger_path = input_root / "parity-ledger.json"
    if ledger_path.is_file():
        ledger = load_json(ledger_path)
        if ledger.get("schema") != PARITY_LEDGER_SCHEMA:
            raise ReferenceError(f"{ledger_path.name} has an unexpected schema")
        summary["parity_ledger"] = {
            "classification_counts": ledger["classification_counts"],
            "entry_count": ledger["entry_count"],
            "ledger_sha256": sha256_file(ledger_path)[0],
        }

    install_path = input_root / "install.manifest.json"
    install_diff_path = input_root / "retail-to-install.diff.json"
    if install_path.is_file() and install_diff_path.is_file():
        install = load_json(install_path)
        install_diff = load_json(install_diff_path)
        validate_manifest(install, "install input")
        if install_diff.get("schema") != DIFF_SCHEMA:
            raise ReferenceError(f"{install_diff_path.name} has an unexpected schema")
        summary["local_installation"] = {
            "file_count": install["file_count"],
            "manifest_sha256": sha256_file(install_path)[0],
            "retail_diff_sha256": sha256_file(install_diff_path)[0],
            "retail_diff_summary": install_diff["summary"],
            "total_bytes": install["total_bytes"],
        }

    artifacts_path = input_root / "standalone-artifacts.json"
    if artifacts_path.is_file():
        artifacts = load_json(artifacts_path)
        if artifacts.get("schema") != FILES_SCHEMA:
            raise ReferenceError(f"{artifacts_path.name} has an unexpected schema")
        summary["standalone_artifacts"] = [
            {
                "name": entry["name"],
                "sha256": entry["sha256"],
                "size": entry["size"],
            }
            for entry in artifacts["artifacts"]
        ]

    external_pe_path = input_root / "external-patch.pe.json"
    if external_pe_path.is_file():
        external_pe = load_json(external_pe_path)
        if external_pe.get("schema") != PE_SCHEMA:
            raise ReferenceError(f"{external_pe_path.name} has an unexpected schema")
        summary["external_patch_pe"] = summarized_pe(external_pe)

    return summary


def ensure_output_outside_root(root: Path, output: Path) -> None:
    root = root.resolve()
    output = output.resolve()
    try:
        output.relative_to(root)
    except ValueError:
        return
    raise ReferenceError("manifest output must be outside the tree being inventoried")


def command_manifest(args: argparse.Namespace) -> int:
    root = Path(args.root)
    output = Path(args.output)
    ensure_output_outside_root(root, output)
    manifest = build_manifest(
        root,
        args.label,
        parse_text_extensions(args.text_extensions),
        args.exclude,
    )
    write_json(output, manifest)
    print(
        f"manifest {args.label}: {manifest['file_count']} files, "
        f"{manifest['total_bytes']} bytes -> {output}"
    )
    return 0


def command_diff(args: argparse.Namespace) -> int:
    left = load_json(Path(args.left))
    right = load_json(Path(args.right))
    report = build_manifest_diff(left, right)
    write_json(Path(args.output), report)
    summary = report["summary"]
    print(
        f"diff {left['root_label']} -> {right['root_label']}: "
        f"common={summary['common']} exact={summary['exact']} "
        f"normalized={summary['normalized_equal']} changed={summary['changed']} "
        f"left-only={summary['left_only']} right-only={summary['right_only']}"
    )
    return 0


def command_verify(args: argparse.Namespace) -> int:
    expected = load_json(Path(args.manifest))
    validate_manifest(expected, "expected input")
    normalization = expected.get("normalization", {})
    text_extensions = frozenset(normalization.get("text_extensions", []))
    actual = build_manifest(
        Path(args.root),
        expected["root_label"],
        text_extensions,
        expected.get("excluded_patterns", []),
    )
    report = build_manifest_diff(expected, actual)
    expected_by_path = {entry["path"]: entry for entry in expected["files"]}
    actual_by_path = {entry["path"]: entry for entry in actual["files"]}
    metadata_mismatches = [
        path
        for path in sorted(set(expected_by_path) & set(actual_by_path), key=str.casefold)
        if expected_by_path[path] != actual_by_path[path]
    ]
    identical = actual == expected
    report["verification"] = {
        "manifest_equal": identical,
        "metadata_mismatch_count": len(metadata_mismatches),
        "metadata_mismatch_paths": metadata_mismatches,
    }
    if args.output:
        write_json(Path(args.output), report)
    summary = report["summary"]
    print(
        f"verify {expected['root_label']}: "
        + ("OK" if identical else "MISMATCH")
        + f" ({expected['file_count']} expected files, "
        + f"metadata-mismatches={len(metadata_mismatches)})"
    )
    return 0 if identical else 3


def command_pe(args: argparse.Namespace) -> int:
    report = inspect_pe(Path(args.file), [parse_rva(value) for value in args.probe_rva])
    write_json(Path(args.output), report)
    print(
        f"PE {report['file']['name']}: {report['coff']['machine_name']} "
        f"sections={report['coff']['section_count']} imports={len(report['imports'])} "
        f"-> {args.output}"
    )
    return 0


def command_files(args: argparse.Namespace) -> int:
    report = build_file_artifacts([Path(value) for value in args.file], args.label)
    write_json(Path(args.output), report)
    print(f"artifacts {args.label}: {len(report['artifacts'])} files -> {args.output}")
    return 0


def command_summary(args: argparse.Namespace) -> int:
    report = build_m0_summary(Path(args.input_root))
    if args.source_revision:
        if "source_tree" not in report:
            raise ReferenceError("source revision supplied but source-tree manifest is absent")
        report["source_tree"]["git_revision"] = args.source_revision
    write_json(Path(args.output), report)
    print(
        f"M0 public summary: source={report['source']['file_count']} "
        f"retail={report['retail']['file_count']} -> {args.output}"
    )
    return 0


def command_ledger(args: argparse.Namespace) -> int:
    diff_report = load_json(Path(args.diff))
    ledger = build_parity_ledger(diff_report)
    write_json(Path(args.output), ledger)
    print(
        f"parity ledger: {ledger['entry_count']} unresolved entries -> {args.output}"
    )
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    manifest = subparsers.add_parser("manifest", help="hash a directory tree")
    manifest.add_argument("--root", required=True)
    manifest.add_argument("--label", required=True)
    manifest.add_argument("--output", required=True)
    manifest.add_argument(
        "--text-extensions",
        default=",".join(sorted(DEFAULT_TEXT_EXTENSIONS)),
        help="comma-separated extensions receiving a normalized-text hash",
    )
    manifest.add_argument("--exclude", action="append", default=[], help="relative glob")
    manifest.set_defaults(handler=command_manifest)

    diff = subparsers.add_parser("diff", help="compare two file manifests")
    diff.add_argument("--left", required=True)
    diff.add_argument("--right", required=True)
    diff.add_argument("--output", required=True)
    diff.set_defaults(handler=command_diff)

    verify = subparsers.add_parser("verify", help="verify a tree against a manifest")
    verify.add_argument("--manifest", required=True)
    verify.add_argument("--root", required=True)
    verify.add_argument("--output")
    verify.set_defaults(handler=command_verify)

    pe = subparsers.add_parser("pe", help="inspect a PE file without executing it")
    pe.add_argument("--file", required=True)
    pe.add_argument("--output", required=True)
    pe.add_argument("--probe-rva", action="append", default=[])
    pe.set_defaults(handler=command_pe)

    files = subparsers.add_parser("files", help="hash standalone artifacts")
    files.add_argument("--file", action="append", required=True)
    files.add_argument("--label", required=True)
    files.add_argument("--output", required=True)
    files.set_defaults(handler=command_files)

    summary = subparsers.add_parser(
        "summary", help="export a redacted public summary from a complete M0 run"
    )
    summary.add_argument("--input-root", required=True)
    summary.add_argument("--output", required=True)
    summary.add_argument(
        "--source-revision",
        help="Git revision represented by source-tree.manifest.json",
    )
    summary.set_defaults(handler=command_summary)

    ledger = subparsers.add_parser(
        "ledger", help="create stable parity IDs for every non-identical diff entry"
    )
    ledger.add_argument("--diff", required=True)
    ledger.add_argument("--output", required=True)
    ledger.set_defaults(handler=command_ledger)

    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return int(args.handler(args))
    except (OSError, ReferenceError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
