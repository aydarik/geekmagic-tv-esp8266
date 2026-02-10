Import("env")
import subprocess
import os

SRC = "data/index.html"
DST_DIR = "src/generated"
DST = f"{DST_DIR}/index_html.h"

def build_html(source, target, env):
    os.makedirs(DST_DIR, exist_ok=True)

    cmd = f"xxd -i {SRC} {DST}"
    subprocess.check_call(cmd, shell=True)

    print("✓ index.html converted to header")

env.AddPreAction("buildprog", build_html)
