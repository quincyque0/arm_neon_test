#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
APP="${PROJ_DIR}/build/bin/arm_project_simple"

if [ ! -x "$APP" ]; then
    echo "[run] Бинарь не найден ($APP). Сначала запустите build.sh."
    exit 1
fi
cd "${PROJ_DIR}"
exec "$APP" "$@"