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

"""Prepares a d7 checkout for `make`, `msbuild d7.sln` and `dotnet`.

Lists what it would download (protoc; the .NET SDK unless the machine
has a suitable one), asks once, then
fetches each pinned file, checks its hash and installs it inside the
checkout. Nothing is written outside it. Standard library only, Python
3.9 and later.
"""

import argparse
import hashlib
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.error
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(ROOT, ".cache")
GEN = os.path.join(ROOT, "gen")
DOTNET_HOME = os.path.join(ROOT, "third_party", "dotnet")
PROTOS = [
  "photonscore/proto/base.proto",
  "photonscore/lib/d7/backend/seven/seven.proto",
]
PORT_DEF = "third_party/protobuf/src/google/protobuf/port_def.inc"

PROTOC_URL = ("https://github.com/protocolbuffers/protobuf/releases/"
              "download/v{0}/protoc-{0}-{1}.zip")
# protoc release zips by version and platform: size, SHA-256.
PROTOC = {
  "3.20.1": {
    "win64": (1544958,
      "897bf86b9c989f91c4171c7f99e3886fedfceb077a94dd150f1401cfe922cd46"),
    "osx-x86_64": (2708249,
      "b4f36b18202d54d343a66eebc9f8ae60809a2a96cc2d1b378137550bbe4cf33c"),
    "osx-aarch_64": (2708249,
      "b362acae78542872bb6aac8dba73aaf0dc6e94991b8b0a065d6c3e703fec2a8b"),
    "linux-x86_64": (1714731,
      "3a0e900f9556fbcac4c3a913a00d07680f0fdf6b990a341462d822247b265562"),
    "linux-aarch_64": (1804837,
      "8a5a51876259f934cd2acc2bc59dba0e9a51bd631a5c37a4b9081d6e4dbc7591"),
  },
}

DOTNET_URL = ("https://builds.dotnet.microsoft.com/dotnet/{0}/{1}/"
              "dotnet-{2}-{1}-{3}.{4}")
# The .NET SDK that builds d7 and the runtime net8.0 tests run on, by RID:
# size, SHA-512 as Microsoft's release metadata publishes it.
DOTNET = {
  "sdk": ("Sdk", "10.0.403", {
    "win-x64": (300745255,
      "7a114c318fa7f1d9822991b534b2814d0e78386ceda816b371f5141acefe741d"
      "87ccb606ede06cadac8f9ac5070e32b708a67bbd1619ff5e5a7858a27874ef48"),
    "osx-arm64": (230759804,
      "5893c66b48ea3674c7da18501059d606e96f3672fabe4d5e41762e692c7c83df"
      "3ee0b2b9dc5a0786191da2dc413d7335158101c14c935ab217f31ac2529770f3"),
    "osx-x64": (238694323,
      "ce95d225b86d2401f111c2aa6df0f5e6855c375b7b34ab4a73df90c56f2b0fc2"
      "a44398cfe455cb689670bc8590012e943b64c7a590122af98cdc067721b12223"),
    "linux-x64": (240199984,
      "b7b3d8461cf98b07d2552ce9199702fa3ad010ac2671a5f8cd5136f7534e50f6"
      "855ba7bbb02b11494563f1d04156276a31cda1103b900e5eecd0d9473dc7b9c5"),
    "linux-arm64": (232484877,
      "0966aa2b70f4677b8319be9df447a78a0dcd6164836425b55a68fd714f4023fe"
      "b87b4cac21b9a996b79472ad54487cd0adcd2a9dfd570e2b2a1d26423a2baa9b"),
  }),
  "runtime": ("Runtime", "8.0.33", {
    "win-x64": (33337136,
      "ceb56fa91c15aa7c465db8cd42ee4e978589c58885cd1f164f8fcde7231ea15c"
      "3cfc0bcddb2e6b452238b3687a3e2bcb3683349b8b92a7172e03154ed280be67"),
    "osx-arm64": (29113192,
      "0be37b64ea1f49651bbaa3dd08e89476919ceb98bd6b39d28883439c16c1d9cb"
      "cccf5cf01f67c913095e9f23235dc621125a551a6e736be4ebddbf060eb82758"),
    "osx-x64": (30746138,
      "ba177c678fce742f991fc5e985b7e269faa231e44d35622be13800cb8b1c757f"
      "fdb83e6e10817cb3c48b64fba73a301c2d41b91ce7f6044e1f679f36a0ea3b4b"),
    "linux-x64": (31318724,
      "b314039d337a8c5c4be6a6f99712a2618cbeb5e1ab7cac7a5c1cbf1c97bd355f"
      "5d116b9932a8cfc604ac7849f74638896eba0534892c6518311908aba29b4f7b"),
    "linux-arm64": (29909664,
      "a9ba25eae72f98e25233ae202d6e4fb6663b2c8da23bcd9d797c89e8b1d01f51"
      "de90f1994dc7101a5fef56db401c05af184c5e251a9bce570adf044c4e3ae5a9"),
  }),
}

# Visual Studio major version to its C++ platform toolset.
VS_TOOLSETS = {"16": "v142", "17": "v143", "18": "v145"}


def fail(message):
  sys.exit("configure.py: " + message)


def rel(path):
  return os.path.relpath(path, ROOT).replace(os.sep, "/")


def machine():
  m = platform.machine().lower()
  if m in ("arm64", "aarch64"):
    return "arm64"
  if m in ("amd64", "x86_64"):
    return "x64"
  fail("unsupported machine " + m)


def os_name():
  if sys.platform == "win32":
    return "win"
  if sys.platform == "darwin":
    return "osx"
  if sys.platform.startswith("linux"):
    return "linux"
  fail("unsupported platform " + sys.platform)


def protoc_platform():
  osn, m = os_name(), machine()
  if osn == "win":
    if m != "x64":
      fail("protoc has no Windows arm64 release")
    return "win64"
  return "{}-{}".format(osn, "aarch_64" if m == "arm64" else "x86_64")


def sha(algorithm, path):
  h = hashlib.new(algorithm)
  with open(path, "rb") as f:
    for block in iter(lambda: f.read(1 << 20), b""):
      h.update(block)
  return h.hexdigest()


def run(args):
  try:
    return subprocess.run(args, capture_output=True, text=True,
                          check=True).stdout
  except (OSError, subprocess.CalledProcessError):
    return ""


class Download:
  """A pinned file: where it comes from, what it must hash to, and what
  to do with it once it does."""

  def __init__(self, label, url, size, algorithm, digest, install, into):
    self.label, self.url, self.size = label, url, size
    self.algorithm, self.digest = algorithm, digest
    self.install, self.into = install, into

  def fetch(self):
    os.makedirs(os.path.join(CACHE, "downloads"), exist_ok=True)
    path = os.path.join(CACHE, "downloads", os.path.basename(self.url))
    if os.path.isfile(self.url):
      print("Copying " + self.url)
      shutil.copyfile(self.url, path)
    else:
      print("Downloading " + self.url)
      try:
        with urllib.request.urlopen(self.url) as r, open(path, "wb") as f:
          done, shown = 0, 0
          for block in iter(lambda: r.read(1 << 20), b""):
            f.write(block)
            done += len(block)
            if self.size > 50 << 20 and done * 10 // self.size > shown:
              shown = done * 10 // self.size
              print("  {}%".format(shown * 10), flush=True)
      except (urllib.error.URLError, OSError) as e:
        if os.path.isfile(path):
          os.remove(path)
        fail("downloading {} failed: {}".format(self.url, e))
    got = sha(self.algorithm, path)
    if got != self.digest:
      os.remove(path)
      fail("{} has {} {}, expected {}".format(
        self.url, self.algorithm, got, self.digest))
    self.install(path)
    os.remove(path)


# protoc ---------------------------------------------------------------

def runtime_version():
  with open(os.path.join(ROOT, PORT_DEF)) as f:
    m = re.search(r"#define PROTOBUF_VERSION (\d+)", f.read())
  if m is None:
    fail("no PROTOBUF_VERSION in " + PORT_DEF)
  v = int(m.group(1))
  return "{}.{}.{}".format(v // 1000000, v // 1000 % 1000, v % 1000)


def protoc(version):
  """protoc's path, and its download when it is not in place yet."""
  plat = protoc_platform()
  pin = PROTOC.get(version, {}).get(plat)
  if pin is None:
    fail("no pinned protoc {} for {}; add it to PROTOC".format(version, plat))
  home = os.path.join(CACHE, "protoc-{}-{}".format(version, plat))
  exe = os.path.join(home, "bin",
                     "protoc.exe" if os_name() == "win" else "protoc")
  if run([exe, "--version"]).split() == ["libprotoc", version]:
    return exe, None

  def install(path):
    shutil.rmtree(home, ignore_errors=True)
    with zipfile.ZipFile(path) as z:
      z.extractall(home)
    os.chmod(exe, 0o755)

  return exe, Download("protoc {} ({})".format(version, plat),
                       PROTOC_URL.format(version, plat), pin[0], "sha256",
                       pin[1], install, CACHE)


def generate(exe):
  """protoc into a scratch folder; only changed files replace gen/'s, so
  make and MSBuild rebuild nothing after a rerun."""
  tmp = os.path.join(CACHE, "gen")
  shutil.rmtree(tmp, ignore_errors=True)
  os.makedirs(tmp)
  subprocess.run([exe, "-I.", "--cpp_out=" + tmp] + PROTOS, cwd=ROOT,
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
      print("Generated " + rel(dst))
  shutil.rmtree(tmp)


# .NET -----------------------------------------------------------------

def dotnet_parts(exe):
  """Which of the SDK 10+ and the runtime 8.0 a dotnet has."""
  sdk = any(int(line.split(".")[0]) >= 10
            for line in run([exe, "--list-sdks"]).splitlines()
            if line[:1].isdigit())
  rt8 = any(line.startswith("Microsoft.NETCore.App 8.")
            for line in run([exe, "--list-runtimes"]).splitlines())
  return {"sdk": sdk, "runtime": rt8}


def dotnet():
  """The dotnet to use, and the downloads still needed for it: the
  checkout's own one first, then the machine's, else a new own one."""
  exe_name = "dotnet.exe" if os_name() == "win" else "dotnet"
  own = os.path.join(DOTNET_HOME, exe_name)
  if not os.path.isfile(own):
    system = shutil.which("dotnet")
    if system and all(dotnet_parts(system).values()):
      return system, []
    parts = {"sdk": False, "runtime": False}
  else:
    parts = dotnet_parts(own)
  rid = "{}-{}".format(os_name(), machine())
  ext = "zip" if os_name() == "win" else "tar.gz"
  todo = []
  for part, present in parts.items():
    if present:
      continue
    folder, version, pins = DOTNET[part]
    if rid not in pins:
      fail("no pinned .NET {} for {}".format(part, rid))
    size, digest = pins[rid]
    todo.append(Download(
      ".NET {} {} ({})".format("SDK" if part == "sdk" else "runtime",
                               version, rid),
      DOTNET_URL.format(folder, version, part, rid, ext), size, "sha512",
      digest, extract_into_dotnet_home, DOTNET_HOME))
  return own, todo


def extract_into_dotnet_home(path):
  os.makedirs(DOTNET_HOME, exist_ok=True)
  if path.endswith(".zip"):
    with zipfile.ZipFile(path) as z:
      z.extractall(DOTNET_HOME)
    return
  # Pinned and hashed: trusted, and filter= exists from 3.12 on only
  kw = {"filter": "fully_trusted"} if hasattr(tarfile, "data_filter") else {}
  with tarfile.open(path, "r:gz") as t:
    t.extractall(DOTNET_HOME, **kw)


# Visual Studio --------------------------------------------------------

def find_vs():
  pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
  vswhere = os.path.join(pf86, "Microsoft Visual Studio", "Installer",
                         "vswhere.exe")
  if not os.path.isfile(vswhere):
    fail("vswhere.exe not found; install Visual Studio 2019 or later "
         "with the C++ desktop workload")
  out = run([vswhere, "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
             "-property", "installationVersion"]).strip()
  if not out:
    fail("no Visual Studio with the C++ x64 tools found")
  toolset = VS_TOOLSETS.get(out.split(".")[0])
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
  print("Wrote " + rel(path))


# ----------------------------------------------------------------------

def megabytes(n):
  return "{:.1f} MB".format(n / 1e6)


def confirm(todo, yes):
  width = max(len(d.label) for d in todo)
  print("configure.py will download, into this checkout only:")
  for d in todo:
    print("  - {}  {:>9}  -> {}/".format(d.label.ljust(width),
                                         megabytes(d.size), rel(d.into)))
  print("  {}  {:>9}".format("total".rjust(width + 2),
                             megabytes(sum(d.size for d in todo))))
  if yes:
    return
  if not sys.stdin.isatty():
    fail("not a terminal: pass --yes to allow the downloads")
  try:
    answer = input("Proceed? [y/N] ")
  except EOFError:  # Windows calls NUL a terminal
    answer = ""
  if answer.strip().lower() not in ("y", "yes"):
    fail("nothing downloaded")


def main():
  p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  p.add_argument("--no-dotnet", action="store_true",
                 help="skip .NET: build the native library only")
  p.add_argument("--yes", action="store_true",
                 help="download without asking (CI)")
  args = p.parse_args()

  version = runtime_version()
  protoc_exe, protoc_download = protoc(version)
  todo = [protoc_download] if protoc_download else []
  dotnet_exe = None
  if not args.no_dotnet:
    dotnet_exe, dotnet_downloads = dotnet()
    todo += dotnet_downloads

  if todo:
    confirm(todo, args.yes)
    for d in todo:
      d.fetch()
    if dotnet_exe and not all(dotnet_parts(dotnet_exe).values()):
      fail(rel(dotnet_exe) + " lacks the SDK 10 or the runtime 8.0")

  print("protoc {}: {}".format(version, rel(protoc_exe)))
  generate(protoc_exe)
  if dotnet_exe:
    shown = (rel(dotnet_exe) if dotnet_exe.startswith(ROOT)
             else dotnet_exe)
    print("dotnet: " + shown)
  if os_name() == "win":
    vs, toolset = find_vs()
    print("Visual Studio {}, toolset {}".format(vs, toolset))
    write_local_props(toolset)
    print("Next: msbuild d7.sln -m -p:Configuration=Release -p:Platform=x64")
  else:
    print("Next: make")


if __name__ == "__main__":
  main()
