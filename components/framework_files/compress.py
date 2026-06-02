#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Jon Irons <jon@irons.ws>
# SPDX-License-Identifier: MIT
#
# compress.py <input> <output>
# Compresses <input> with gzip level 9 and writes the result to <output>.
# Used by the framework_files CMakeLists.txt build step to pre-compress
# embedded web assets on all platforms (including Windows, where the gzip
# command-line tool is not available).

import gzip
import pathlib
import sys

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <input> <output>", file=sys.stderr)
    sys.exit(1)

src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])

dst.parent.mkdir(parents=True, exist_ok=True)
dst.write_bytes(gzip.compress(src.read_bytes(), compresslevel=9))
