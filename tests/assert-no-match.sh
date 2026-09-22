#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Unlike a bare ! grep under set -e, a match or read error is fatal to callers.
require_no_match() {
  local status
  if grep "$@"; then
    printf '%s\n' 'Unexpected diagnostic or forbidden source pattern' >&2
    return 1
  else
    status=$?
    if (( status != 1 )); then return "$status"; fi
  fi
}
