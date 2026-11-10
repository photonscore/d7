#!/usr/bin/env bash
# Copyright (c) 2026 Photonscore GmbH
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Fetches pinned CPythons into third_party/python/<X.Y>/, asking first:
#   ./bootstrap.sh [--all] [--yes] [X.Y ...]
# Then: third_party/python/3.12/bin/python configure.py
# Writes nothing outside the checkout. Bash 3.2 (macOS) and later.

set -euo pipefail
cd "$(dirname "$0")"

RELEASE=20261026
URL=https://github.com/astral-sh/python-build-standalone/releases/download
DEFAULT=3.12
ALL="3.11 3.12 3.13 3.14 3.15"
PINS=third_party/python/PINS

fail() { echo "bootstrap.sh: $*" >&2; exit 1; }

versions="" yes=0
for arg in "$@"; do
  case "$arg" in
    --all) versions="$ALL" ;;
    --yes) yes=1 ;;
    [0-9]*.[0-9]*) versions="$versions $arg" ;;
    *) fail "usage: ./bootstrap.sh [--all] [--yes] [X.Y ...]" ;;
  esac
done
[ -n "$versions" ] || versions=$DEFAULT

case "$(uname -s)-$(uname -m)" in
  Linux-x86_64) target=x86_64-unknown-linux-gnu ;;
  Linux-aarch64|Linux-arm64) target=aarch64-unknown-linux-gnu ;;
  Darwin-arm64) target=aarch64-apple-darwin ;;
  Darwin-x86_64) target=x86_64-apple-darwin ;;
  *) fail "no pinned Python for $(uname -s) $(uname -m)" ;;
esac

if command -v sha256sum > /dev/null; then sha256() { sha256sum "$1"; }
else sha256() { shasum -a 256 "$1"; }
fi

# What is missing: one "version sha256 size file" line each
todo="" total=0
for v in $versions; do
  [ -x "third_party/python/$v/bin/python3" ] && continue
  line=$(awk -v v="$v" -v t="$target" '$1 == v && $2 == t' "$PINS")
  [ -n "$line" ] || fail "no pinned Python $v for $target in $PINS"
  set -- $line
  todo="$todo$v $3 $4 $5
"
  total=$((total + $4))
done

if [ -z "$todo" ]; then
  for v in $versions; do
    echo "Python $v: third_party/python/$v/bin/python"
  done
  exit 0
fi

mb() { awk -v n="$1" 'BEGIN { printf "%.1f MB", n / 1e6 }'; }
echo "bootstrap.sh will download, into this checkout only:"
echo "$todo" | while read -r v sha size file; do
  [ -n "$v" ] || continue
  printf "  - %-40s %9s  -> third_party/python/%s/\n" \
    "Python $v ($target)" "$(mb "$size")" "$v"
done
printf "  %42s %9s\n" total "$(mb $total)"
if [ $yes = 0 ]; then
  [ -t 0 ] || fail "not a terminal: pass --yes to allow the downloads"
  read -r -p "Proceed? [y/N] " answer
  case "$answer" in y|Y|yes) ;; *) fail "nothing downloaded" ;; esac
fi

mkdir -p .cache/downloads
echo "$todo" | while read -r v sha size file; do
  [ -n "$v" ] || continue
  archive=.cache/downloads/$file
  echo "Downloading $file"
  curl -fsSL -o "$archive" "$URL/$RELEASE/$file"
  got=$(sha256 "$archive" | cut -d' ' -f1)
  if [ "$got" != "$sha" ]; then
    rm -f "$archive"
    fail "$file has SHA-256 $got, expected $sha"
  fi
  rm -rf ".cache/python-$v" "third_party/python/$v"
  mkdir -p ".cache/python-$v"
  tar -xzf "$archive" -C ".cache/python-$v"
  mv ".cache/python-$v/python" "third_party/python/$v"
  rm -rf ".cache/python-$v" "$archive"
  echo "Python $v: third_party/python/$v/bin/python"
done
