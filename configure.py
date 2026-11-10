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

"""Prepares a d7 checkout for `make` (Unix) or `msbuild d7.sln` (Windows).

Fetches the protoc matching the vendored protobuf runtime, generates the
C++ code of the D7 protos into `gen/` and, on Windows, writes
`msvc/local.props` for the Visual Studio found by vswhere. Standard
library only; runs on Python 3.9 and later.
"""

import hashlib
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(ROOT, ".cache")
GEN = os.path.join(ROOT, "gen")
PROTOS = [
  "photonscore/proto/base.proto",
  "photonscore/lib/d7/backend/seven/seven.proto",
]
PORT_DEF = "third_party/protobuf/src/google/protobuf/port_def.inc"
PROTOC_URL = ("https://github.com/protocolbuffers/protobuf/releases/"
              "download/v{0}/protoc-{0}-{1}.zip")

# SHA-256 of every protoc release zip we accept, by version and platform.
PROTOC_SHA256 = {
  "3.20.1": {
    "win64":
      "897bf86b9c989f91c4171c7f99e3886fedfceb077a94dd150f1401cfe922cd46",
    "osx-x86_64":
      "b4f36b18202d54d343a66eebc9f8ae60809a2a96cc2d1b378137550bbe4cf33c",
    "osx-aarch_64":
      "b362acae78542872bb6aac8dba73aaf0dc6e94991b8b0a065d6c3e703fec2a8b",
    "linux-x86_64":
      "3a0e900f9556fbcac4c3a913a00d07680f0fdf6b990a341462d822247b265562",
    "linux-aarch_64":
      "8a5a51876259f934cd2acc2bc59dba0e9a51bd631a5c37a4b9081d6e4dbc7591",
  },
}

# Visual Studio major version to its C++ platform toolset.
VS_TOOLSETS = {"16": "v142", "17": "v143", "18": "v145"}


def fail(message):
  sys.exit("configure.py: " + message)


def runtime_version():
  with open(os.path.join(ROOT, PORT_DEF)) as f:
    m = re.search(r"#define PROTOBUF_VERSION (\d+)", f.read())
  if m is None:
    fail("no PROTOBUF_VERSION in " + PORT_DEF)
  v = int(m.group(1))
  return "{}.{}.{}".format(v // 1000000, v // 1000 % 1000, v % 1000)


def protoc_platform():
  machine = platform.machine().lower()
  arm = machine in ("arm64", "aarch64")
  if sys.platform == "win32" and machine in ("amd64", "x86_64"):
    return "win64"
  if sys.platform == "darwin":
    return "osx-aarch_64" if arm else "osx-x86_64"
  if sys.platform.startswith("linux") and (arm or machine == "x86_64"):
    return "linux-aarch_64" if arm else "linux-x86_64"
  fail("no protoc release for {} {}".format(sys.platform, machine))


def protoc_reports(exe, version):
  if not os.path.isfile(exe):
    return False
  try:
    out = subprocess.run([exe, "--version"], capture_output=True,
                         text=True, check=True).stdout
  except (OSError, subprocess.CalledProcessError):
    return False
  return out.split() == ["libprotoc", version]


def fetch_protoc(version):
  plat = protoc_platform()
  sha = PROTOC_SHA256.get(version, {}).get(plat)
  if sha is None:
    fail("no pinned protoc {} for {}; add its SHA-256".format(version, plat))
  home = os.path.join(CACHE, "protoc-{}-{}".format(version, plat))
  exe = os.path.join(home, "bin",
                     "protoc.exe" if sys.platform == "win32" else "protoc")
  if protoc_reports(exe, version):
    return exe
  url = PROTOC_URL.format(version, plat)
  print("Downloading " + url)
  with urllib.request.urlopen(url) as r:
    data = r.read()
  got = hashlib.sha256(data).hexdigest()
  if got != sha:
    fail("{} has SHA-256 {}, expected {}".format(url, got, sha))
  shutil.rmtree(home, ignore_errors=True)
  os.makedirs(home)
  with tempfile.TemporaryFile() as tmp:
    tmp.write(data)
    with zipfile.ZipFile(tmp) as z:
      z.extractall(home)
  os.chmod(exe, 0o755)
  if not protoc_reports(exe, version):
    fail(exe + " does not report libprotoc " + version)
  return exe


def generate(protoc):
  """Runs protoc into a scratch folder, then replaces only changed files,
  so make and MSBuild rebuild nothing after a rerun."""
  with tempfile.TemporaryDirectory() as tmp:
    subprocess.run([protoc, "-I.", "--cpp_out=" + tmp] + PROTOS, cwd=ROOT,
                   check=True)
    for base, _, files in os.walk(tmp):
      for name in files:
        src = os.path.join(base, name)
        dst = os.path.join(GEN, os.path.relpath(src, tmp))
        with open(src, "rb") as f:
          new = f.read()
        if os.path.isfile(dst):
          with open(dst, "rb") as f:
            if f.read() == new:
              continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as f:
          f.write(new)
        print("Generated " + os.path.relpath(dst, ROOT))


def find_vs():
  pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
  vswhere = os.path.join(pf86, "Microsoft Visual Studio", "Installer",
                         "vswhere.exe")
  if not os.path.isfile(vswhere):
    fail("vswhere.exe not found; install Visual Studio 2019 or later "
         "with the C++ desktop workload")
  out = subprocess.run(
    [vswhere, "-latest", "-products", "*", "-requires",
     "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
     "-property", "installationVersion"],
    capture_output=True, text=True, check=True).stdout.strip()
  if not out:
    fail("no Visual Studio with the C++ x64 tools found")
  major = out.split(".")[0]
  toolset = VS_TOOLSETS.get(major)
  if toolset is None:
    fail("Visual Studio {} has no known toolset; extend VS_TOOLSETS"
         .format(out))
  return out, toolset


def write_local_props(toolset):
  path = os.path.join(ROOT, "msvc", "local.props")
  text = (
    '<?xml version="1.0" encoding="utf-8"?>\n'
    '<!-- Written by configure.py; not tracked. -->\n'
    '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n'
    '  <PropertyGroup>\n'
    '    <PlatformToolset>{}</PlatformToolset>\n'
    '    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>\n'
    '  </PropertyGroup>\n'
    '</Project>\n').format(toolset)
  with open(path, "w", newline="\r\n") as f:
    f.write(text)
  print("Wrote " + os.path.relpath(path, ROOT))


def main():
  version = runtime_version()
  protoc = fetch_protoc(version)
  print("protoc {}: {}".format(version, os.path.relpath(protoc, ROOT)))
  generate(protoc)
  if sys.platform == "win32":
    vs, toolset = find_vs()
    print("Visual Studio {}, toolset {}".format(vs, toolset))
    write_local_props(toolset)
    print("Next: msbuild d7.sln -p:Configuration=Release -p:Platform=x64")
  else:
    print("Next: make")


if __name__ == "__main__":
  main()
