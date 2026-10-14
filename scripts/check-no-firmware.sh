#!/bin/sh
# Fail if proprietary firmware (or the third-party reference tree) is tracked.
# Run it any time; the pre-commit hook in scripts/hooks calls the same idea.
set -e
bad=$(git ls-files | grep -Ei '\.(syx|whx|whd)$' || true)
if [ -n "$bad" ]; then
    echo "Firmware files must not be tracked:" >&2
    printf '%s\n' "$bad" >&2
    exit 1
fi
if git ls-files | grep -q '^Smallest-Doom-rp2040/'; then
    echo "The third-party reference tree must not be tracked." >&2
    exit 1
fi
echo "OK: no firmware or third-party reference files tracked."
