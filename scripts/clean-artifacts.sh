#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# holonightd - Lightweight daemon for routine maintenance automation.
# Copyright (C) 2026 holonightd contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

set -euo pipefail

mode="${1:---dry-run}"

usage() {
  cat <<'EOF'
Usage: scripts/clean-artifacts.sh [--dry-run|--delete]

Remove local generated build directories and root-level log files that are
already ignored by Git.

Modes:
  --dry-run  Print what would be removed.
  --delete   Remove the ignored artifacts.
EOF
}

if [[ "${mode}" == "--help" || "${mode}" == "-h" ]]; then
  usage
  exit 0
fi

if [[ "${mode}" != "--dry-run" && "${mode}" != "--delete" ]]; then
  echo "ERROR: unknown mode: ${mode}" >&2
  usage >&2
  exit 1
fi

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

shopt -s nullglob

candidates=(
  build
  build-*
  cmake-build-*
  compile_commands.json
  *.log
)

artifacts=()
for candidate in "${candidates[@]}"; do
  if [[ -e "${candidate}" && $(git check-ignore -q -- "${candidate}"; echo $?) -eq 0 ]]; then
    artifacts+=("${candidate}")
  fi
done

if (( ${#artifacts[@]} == 0 )); then
  echo "No ignored build/log artifacts found."
  exit 0
fi

if [[ "${mode}" == "--dry-run" ]]; then
  echo "Would remove:"
  printf '  %s\n' "${artifacts[@]}"
  exit 0
fi

echo "Removing:"
printf '  %s\n' "${artifacts[@]}"
rm -rf -- "${artifacts[@]}"
