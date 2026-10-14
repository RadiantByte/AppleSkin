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

    def tables(self, name, slot_count):
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
                        if self.pointer(type_reference - 8) != 0:
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


def source_calls():
    path = Path(__file__).resolve().parent.parent / "include/appleskin/Build.hpp"
    source = path.read_text(encoding="utf-8")
    block = re.search(r"namespace call \{(.*?)\n\}", source, re.S).group(1)
    return {name: int(value, 0) for name, value in re.findall(r"std::size_t (\w+) = (\w+);", block)}


def report(image):
    tables = {}
    for name, count in [("17HudHungerRenderer", 18), ("16HudHeartRenderer", 18),
                        ("23HungerAttributeDelegate", 7), ("4Item", 56),
                        ("24MinecraftUIRenderContext", 33), ("14ClientInstance", 176),
                        ("11LocalPlayer", 79), ("23FoodItemComponentLegacy", 8),
                        ("10BitmapFont", 10), ("5Level", 345)]:
        tables[name] = image.tables(name, count)
    result = {"identity": image.identity(), "rtti": tables,
              "call_offsets_from_current_source": source_calls(), "decoded": {}}
    def anchor(type_name, slot):
        found = tables[type_name]
        return found[0]["slots"][slot] if len(found) == 1 else None

    anchors = {"HeartTick": anchor("16HudHeartRenderer", 4),
               "HeartRender": anchor("16HudHeartRenderer", 17),
               "Delegate": anchor("23HungerAttributeDelegate", 2),
               "Tick": anchor("17HudHungerRenderer", 4),
               "Render": anchor("17HudHungerRenderer", 17)}
    result["anchors"] = anchors
    for name, offset in source_calls().items():
        base = next((value for prefix, value in anchors.items() if name.startswith(prefix)), None)
        if base is None:
            result["decoded"][name] = {"site_rva": None, "target_rva": None}
            continue
        site = base + offset
        try:
            target = image.address_pair(site, 1) if name.endswith("Key") else image.branch(site)
        except ValueError:
            target = None
        result["decoded"][name] = {"site_rva": site, "target_rva": target}
    result["mapping_note"] = "Call offsets describe the current source; new builds require semantic remapping"
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--disassemble", type=lambda text: int(text, 0))
    parser.add_argument("--bytes", default=0x200, type=lambda text: int(text, 0))
    args = parser.parse_args()
    image = MinecraftImage(args.library.resolve())
    if args.disassemble is not None:
        from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
        start = image.file_offset(args.disassemble)
        for instruction in Cs(CS_ARCH_ARM64, CS_MODE_ARM).disasm(image.bytes[start:start + args.bytes], args.disassemble):
            print(f"{instruction.address:#012x}  {instruction.mnemonic:10} {instruction.op_str}")
        return
    text = json.dumps(report(image), indent=2) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
        print(args.output.resolve())
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
