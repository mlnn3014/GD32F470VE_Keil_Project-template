import argparse
import binascii
import struct
from pathlib import Path

OTA_MAGIC = 0x5AA5C33C
OTA_VERSION = 0x00010001


def pack_raw_ota(bin_path: Path) -> Path:
    data = bin_path.read_bytes()
    crc = binascii.crc32(data) & 0xFFFFFFFF
    header = struct.pack("<IIII", OTA_MAGIC, OTA_VERSION, len(data), crc)

    out_path = bin_path.with_name(f"{bin_path.stem}_raw_ota.bin")
    out_path.write_bytes(header + data)
    return out_path


def main() -> None:
    parser = argparse.ArgumentParser(description="pack app bin for raw uart ota")
    parser.add_argument("bin", help="App.bin path")
    args = parser.parse_args()

    bin_path = Path(args.bin)
    if not bin_path.exists():
        raise SystemExit(f"not found: {bin_path}")

    out_path = pack_raw_ota(bin_path)
    print(out_path)


if __name__ == "__main__":
    main()
