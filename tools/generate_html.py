#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path

COMMANDS = [
    "gzip -9 -c res/index.html > src/generated/index.html.gz",
    "xxd -i src/generated/index.html.gz > src/generated/index_html.h",
    "sed -i 's/unsigned char/const unsigned char PROGMEM/' src/generated/index_html.h",
    "rm src/generated/index.html.gz",
    "gzip -9 -c res/ota.html > src/generated/ota.html.gz",
    "xxd -i src/generated/ota.html.gz > src/generated/ota_html.h",
    "sed -i 's/unsigned char/const unsigned char PROGMEM/' src/generated/ota_html.h",
    "rm src/generated/ota.html.gz",
]

def main() -> None:
    project_root = Path(__file__).resolve().parent.parent
    os.chdir(project_root)

    os.makedirs("src/generated", exist_ok=True)

    for cmd in COMMANDS:
        print(f"Running: {cmd}")
        subprocess.run(cmd, shell=True, check=True)
    print("Done.")


if __name__ == "__main__":
    main()
