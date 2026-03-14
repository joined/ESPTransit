#!/usr/bin/env bash
# Wrapper around idf.py that handles board selection and IDF environment setup.
#
# Usage: scripts/idf.sh <board> [idf.py args...]
#   board  - board name (default: jc8012p4a1c)
#
# Example: scripts/idf.sh jc4880p443c build flash monitor
set -e

if [ -z "$IDF_PATH" ]; then
	for candidate in "$HOME/esp/esp-idf" "$HOME/.espressif/esp-idf" "/opt/esp-idf"; do
		if [ -f "$candidate/export.sh" ]; then
			IDF_PATH="$candidate"
			break
		fi
	done
fi

if [ -z "$IDF_PATH" ]; then
	echo "IDF_PATH is not set and esp-idf could not be found in common locations." >&2
	echo "Set IDF_PATH to your esp-idf installation directory." >&2
	exit 1
fi

BOARD="${1:-jc8012p4a1c}"
shift

# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" >/dev/null

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../esp"
exec idf.py -B "build_${BOARD}" -DBOARD="${BOARD}" "$@"
