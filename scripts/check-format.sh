#!/usr/bin/env bash
set -euo pipefail

find . \( -iname '*.cpp' -o -iname '*.cc' -o -iname '*.cxx' \
         -o -iname '*.hpp' -o -iname '*.h' -o -iname '*.ipp' \) \
     -type f -print0 |
{
  failed=0
  while IFS= read -r -d '' file; do
    if ! clang-format --dry-run --Werror "$file"; then
      failed=1
      printf 'clang-format falló en: %s\n' "$file" >&2
    fi
  done

  if [[ "$failed" -ne 0 ]]; then
    exit 1
  fi
}

if [[ -f main.cpp ]] && ! clang-format --dry-run --Werror main.cpp;; then
  exit 1
fi