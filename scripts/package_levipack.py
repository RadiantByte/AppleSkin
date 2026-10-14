import argparse
import json
import zipfile
from pathlib import Path


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--icon", type=Path, default=root / "assets" / "icon.png")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    library = args.library.resolve()
    icon = args.icon.resolve()
    license_file = root / "LICENSE"
    if not library.is_file():
        raise FileNotFoundError(library)
    if not icon.is_file():
        raise FileNotFoundError(icon)
    if not license_file.is_file():
        raise FileNotFoundError(license_file)
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    manifest["entry"] = library.name
    manifest["icon"] = "icon.png"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as package:
        package.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
        package.write(library, library.name)
        package.write(icon, "icon.png")
        package.write(license_file, "LICENSE")
    print(args.output.resolve())


if __name__ == "__main__":
    main()
