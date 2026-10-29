import argparse
import hashlib
import json
import mmap
import re
import struct
from pathlib import Path

from elftools.elf.elffile import ELFFile


class MinecraftImage:
    def __init__(self, path):
        self.file = path.open("rb")
        self.elf = ELFFile(self.file)
        if self.elf["e_machine"] != "EM_AARCH64" or self.elf.elfclass != 64:
            raise ValueError("Supply the Android AArch64 Minecraft ELF")
        self.bytes = mmap.mmap(self.file.fileno(), 0, access=mmap.ACCESS_READ)
        self.segments = [s for s in self.elf.iter_segments() if s["p_type"] == "PT_LOAD"]
        self.relocations = {}
        self.references = {}
        for section in self.elf.iter_sections():
            if section["sh_type"] == "SHT_RELR":
                raise ValueError("This helper needs RELR support before mapping this library")
            if section["sh_type"] != "SHT_RELA":
                continue
            for offset in range(section["sh_offset"], section["sh_offset"] + section["sh_size"], 24):
                address, info, target = struct.unpack_from("<QQq", self.bytes, offset)
                if info & 0xFFFFFFFF != 1027:
                    continue
                self.relocations[address] = target
                self.references.setdefault(target, []).append(address)

    def file_offset(self, address):
        for segment in self.segments:
            if segment["p_vaddr"] <= address < segment["p_vaddr"] + segment["p_filesz"]:
                return address - segment["p_vaddr"] + segment["p_offset"]
        raise ValueError(f"No file bytes for virtual address {address:#x}")

    def virtual_address(self, offset):
        for segment in self.segments:
            if segment["p_offset"] <= offset < segment["p_offset"] + segment["p_filesz"]:
                return offset - segment["p_offset"] + segment["p_vaddr"]
        raise ValueError(f"No load segment for file offset {offset:#x}")

    def pointer(self, address):
        if address in self.relocations:
            return self.relocations[address]
        return struct.unpack_from("<Q", self.bytes, self.file_offset(address))[0]

    def instruction(self, address):
        return struct.unpack_from("<I", self.bytes, self.file_offset(address))[0]

    def executable(self, address):
        return any(s["p_flags"] & 1 and s["p_vaddr"] <= address < s["p_vaddr"] + s["p_memsz"] for s in self.segments)

    def tables(self, name, slot_count, top=0):
        result = []
        needle = name.encode("ascii") + b"\0"
        offset = 0
        while (offset := self.bytes.find(needle, offset)) >= 0:
            try:
                address = self.virtual_address(offset)
            except ValueError:
                offset += len(needle)
                continue
            for reference in self.references.get(address, []):
                type_info = reference - 8
                for type_reference in self.references.get(type_info, []):
                    table = type_reference + 8
                    try:
                        if self.pointer(type_reference - 8) != top & 0xffffffffffffffff:
                            continue
                        entries = [self.pointer(table + index * 8) for index in range(slot_count)]
                    except ValueError:
                        continue
                    if not all(self.executable(entry) or entry == 0 for entry in entries):
                        continue
                    result.append({"name_rva": address, "typeinfo_rva": type_info,
                                   "address_point_rva": table, "slots": entries})
            offset += len(needle)
        return result

    def branch(self, address):
        instruction = self.instruction(address)
        if instruction & 0xFC000000 != 0x94000000:
            return None
        displacement = instruction & 0x3FFFFFF
        if displacement & 0x2000000:
            displacement -= 0x4000000
        target = address + displacement * 4
        return target if self.executable(target) else None

    def address_pair(self, address, register):
        adrp = self.instruction(address)
        add = self.instruction(address + 4)
        if adrp & 0x9F00001F != 0x90000000 | register:
            return None
        if add & 0xFFC003FF != 0x91000000 | register << 5 | register:
            return None
        immediate = (adrp >> 5 & 0x7FFFF) * 4 + (adrp >> 29 & 3)
        if immediate & 0x100000:
            immediate -= 0x200000
        return (address & ~0xFFF) + immediate * 4096 + (add >> 10 & 0xFFF)

    def identity(self):
        build_id = None
        for segment in self.elf.iter_segments():
            if segment["p_type"] == "PT_NOTE":
                for note in segment.iter_notes():
                    if note["n_type"] == "NT_GNU_BUILD_ID":
                        build_id = note["n_desc"]
        return {"sha256": hashlib.sha256(self.bytes).hexdigest(), "size": len(self.bytes), "gnu_build_id": build_id}


def source_slots():
    root = Path(__file__).resolve().parent.parent
    text = (root / "include/appleskin/Build.hpp").read_text(encoding="utf-8")
    block = re.search(r"namespace slot \{(.*?)\n\}", text, re.S).group(1)
    return {name: int(value, 0) for name, value in re.findall(r"std::size_t (\w+) = (\w+);", block)}


def source_patterns():
    root = Path(__file__).resolve().parent.parent
    text = (root / "src/runtime/Patterns.hpp").read_text(encoding="utf-8")
    return [(name, [None if byte == "?" else int(byte, 16) for byte in pattern.split()], int(anchor))
            for name, pattern, anchor in re.findall(r'\{"(\w+)", "([? A-F0-9]+)", (\d+)\}', text)]


def scan(image, pattern, anchor):
    word = bytes(pattern[anchor:anchor + 4])
    fixed = [(offset, value) for offset, value in enumerate(pattern) if value is not None]
    hits = []
    for segment in image.segments:
        if not segment["p_flags"] & 1:
            continue
        begin, end = segment["p_offset"], segment["p_offset"] + segment["p_filesz"]
        cursor = begin
        while (found := image.bytes.find(word, cursor, end)) >= 0:
            cursor = found + 1
            start = found - anchor
            address = segment["p_vaddr"] + start - begin
            if start < begin or start + len(pattern) > end or address % 4:
                continue
            if all(image.bytes[start + offset] == value for offset, value in fixed):
                hits.append(address)
                if len(hits) > 1:
                    return hits
    return hits


def make_signature(image, address, size):
    from capstone import Cs, CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN
    from capstone.arm64 import ARM64_OP_IMM, ARM64_OP_MEM
    disassembler = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
    disassembler.detail = True
    start = image.file_offset(address)
    pattern = []
    for instruction in disassembler.disasm(image.bytes[start:start + size], address):
        if instruction.mnemonic in ("adr", "adrp", "b", "bl"):
            pattern.extend([None] * 4)
        elif any(operand.type == ARM64_OP_MEM or (operand.type == ARM64_OP_IMM and operand.imm != 0)
                 for operand in instruction.operands):
            pattern.extend([None, None, None, instruction.bytes[3]])
        else:
            pattern.extend(instruction.bytes)
    anchors = [offset for offset in range(0, len(pattern) - 3, 4) if None not in pattern[offset:offset + 4]]
    if not anchors:
        raise ValueError("Choose a range with a fixed ARM instruction")
    hits = scan(image, pattern, anchors[0])
    return {"pattern": " ".join("?" if byte is None else f"{byte:02X}" for byte in pattern),
            "anchor": anchors[0], "length": len(pattern), "hits": hits, "unique": hits == [address]}


def report(image):
    slots = source_slots()
    rtti = {name: image.tables(name, count) for name, count in [
        ("17HudHungerRenderer", 18), ("16HudHeartRenderer", 18), ("4Item", 56),
        ("24MinecraftUIRenderContext", 34), ("14ClientInstance", 174),
        ("11LocalPlayer", 79), ("23FoodItemComponentLegacy", 5),
        ("10BitmapFont", 10), ("5Level", 345)]}
    rtti["13ComponentItem"] = image.tables("13ComponentItem", 56)
    rtti["17FoodItemComponent.secondary"] = image.tables("17FoodItemComponent", 5, -16)
    patterns = {name: scan(image, pattern, anchor) for name, pattern, anchor in source_patterns()}
    valid = bool(patterns) and all(len(hits) == 1 for hits in patterns.values())
    targets = {}
    if valid:
        at = {name: hits[0] for name, hits in patterns.items()}
        branches = {"lookup": ("HungerRead", 16), "value": ("HungerRead", 20),
                    "maximum": ("HealthRead", 60), "meshValid": ("MeshRead", 20),
                    "position": ("PositionRead", 4), "batch": ("BatchRead", 36),
                    "hasEffect": ("EffectRead", 8), "level": ("LevelRead", 4),
                    "gameRule": ("RuleRead", 32), "exhaustionComponent": ("ComponentRead", 4), "fontDraw": ("NativeText", 40)}
        targets.update({name: image.branch(at[anchor] + offset) for name, (anchor, offset) in branches.items()})
        targets.update({name: image.address_pair(at[anchor], 1) for name, anchor in [
            ("hungerKey", "HungerRead"), ("saturationKey", "SaturationRead"),
            ("healthKey", "HealthRead"), ("exhaustionKey", "ExhaustionRead")]})
        valid &= all(targets.values())
        for name, type_name, slot_name in [("hunger", "17HudHungerRenderer", "HungerRender"),
            ("heart", "16HudHeartRenderer", "HeartRender"), ("hover", "4Item", "ItemHoverText"),
            ("text", "24MinecraftUIRenderContext", "UiDrawText"),
            ("textView", "24MinecraftUIRenderContext", "UiDrawTextView"),
            ("finish", "24MinecraftUIRenderContext", "UiFinish")]:
            tables = rtti[type_name]
            if len(tables) == 1:
                targets[name] = tables[0]["slots"][slots[slot_name]]
            else:
                valid = False
        checks = {"hungerLookup": image.branch(at["SaturationRead"] + 16) == targets["lookup"],
                  "heartLookup": image.branch(at["HealthRead"] + 12) == targets["lookup"],
                  "saturationCurrent": image.branch(at["SaturationRead"] + 20) == targets["value"],
                  "heartCurrent": image.branch(at["HealthRead"] + 48) == targets["value"],
                  "level": image.branch(at["RuleRead"] + 8) == targets["level"],
                  "meshStride": (image.instruction(at["HungerLayout"] + 12) >> 5 & 0xffff) == 0x260,
                  "batchArgument": image.instruction(at["BatchRead"] + 32) == 0xaa1f03e5,
                  "hungerPositions": (image.instruction(at["HungerLayout"] + 20) >> 10 & 0xfff) == 0xf28,
                  "heartPositions": (image.instruction(at["HeartLayout"] + 20) >> 10 & 0xfff) * 8 == 0x32b8,
                  "gpuPointer": (image.instruction(at["HungerHeader"] + 32) >> 10 & 0xfff) * 8 == 0}
        valid &= all(checks.values())
    else:
        checks = {}
    return {"identity": image.identity(), "signature_matches": patterns, "targets": targets,
            "checks": checks, "rtti": rtti, "verified": bool(valid)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--disassemble", type=lambda text: int(text, 0))
    parser.add_argument("--bytes", default=0x200, type=lambda text: int(text, 0))
    parser.add_argument("--signature-at", type=lambda text: int(text, 0))
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    image = MinecraftImage(args.library.resolve())
    if args.signature_at is not None:
        result = make_signature(image, args.signature_at, args.bytes)
        print(json.dumps(result, indent=2))
        if args.verify and not result["unique"]:
            raise SystemExit(1)
        return
    if args.disassemble is not None:
        from capstone import Cs, CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN
        start = image.file_offset(args.disassemble)
        for instruction in Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN).disasm(image.bytes[start:start + args.bytes], args.disassemble):
            print(f"{instruction.address:#012x}  {instruction.mnemonic:10} {instruction.op_str}")
        return
    result = report(image)
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
        print(args.output.resolve())
    else:
        print(text, end="")
    if args.verify and not result["verified"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
