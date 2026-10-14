#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 web/check_memory.py
bash work/check_menu_asan.sh
