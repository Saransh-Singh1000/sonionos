import os
import sys

missing = [p for p in sys.argv[1:] if not os.path.isfile(p) or os.path.getsize(p) == 0]
if missing:
    print("[FAIL] Missing or empty build outputs:")
    for p in missing:
        print(" -", p)
    sys.exit(1)

print("[PASS] All four SonionOS build artifacts exist and are non-empty.")
