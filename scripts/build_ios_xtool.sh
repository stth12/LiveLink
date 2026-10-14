#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
xtool dev build --configuration release --ipa
printf '\nUnsigned iOS archive: %s/xtool/LiveLink.ipa\n' "$PWD"
