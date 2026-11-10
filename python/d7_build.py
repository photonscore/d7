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

"""Builds the photonscore-d7 wheel from a checkout whose shared library is
built (`configure.py`, then `make` or `msbuild d7.sln`). It is also the
PEP 517 backend pyproject.toml names, so `pip install .` works there.

    python python/d7_build.py OUT_DIR

The wheel is py3-none-<platform>: the library is C, so any Python 3 will
do. It never builds the library itself, since configure.py asks first.
"""

import base64
import hashlib
import os
import platform
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
NAME = "photonscore_d7"
DIST = "photonscore-d7"

if sys.platform == "win32":
  LIBRARY = os.path.join(ROOT, "out", "x64", "Release", "photonscore_d7.dll")
elif sys.platform == "darwin":
  LIBRARY = os.path.join(ROOT, "out", "libphotonscore_d7.dylib")
else:
  LIBRARY = os.path.join(ROOT, "out", "libphotonscore_d7.so")


def version():
  with open(os.path.join(ROOT, "VERSION.txt")) as f:
    return f.read().strip()


def platform_tag():
  """D7_WHEEL_PLATFORM overrides; macOS takes MACOSX_DEPLOYMENT_TARGET,
  which the library must have been built with."""
  tag = os.environ.get("D7_WHEEL_PLATFORM")
  if tag:
    return tag
  m = platform.machine().lower()
  arm = m in ("arm64", "aarch64")
  if sys.platform == "win32":
    return "win_amd64"
  if sys.platform == "darwin":
    target = os.environ.get("MACOSX_DEPLOYMENT_TARGET", "11.0")
    return "macosx_{}_{}".format(target.replace(".", "_"),
                                 "arm64" if arm else "x86_64")
  return "linux_{}".format("aarch64" if arm else "x86_64")


def metadata():
  with open(os.path.join(HERE, "README.md"), encoding="utf-8") as f:
    readme = f.read()
  return "\n".join([
    "Metadata-Version: 2.4",
    "Name: " + DIST,
    "Version: " + version(),
    "Summary: Read and write D7 .photons files, Photonscore's format for "
    "time-resolved single-photon data",
    "Author: Photonscore GmbH",
    "License-Expression: Apache-2.0",
    "License-File: LICENSE",
    "License-File: NOTICE",
    "Project-URL: Homepage, https://www.photonscore.de",
    "Project-URL: Source, https://github.com/photonscore/d7",
    "Keywords: photonscore,d7,photons,single-photon,tcspc,flim,lincam,lintag",
    "Classifier: Programming Language :: Python :: 3",
    "Classifier: Operating System :: Microsoft :: Windows",
    "Classifier: Operating System :: MacOS",
    "Classifier: Operating System :: POSIX :: Linux",
    "Classifier: Topic :: Scientific/Engineering :: Physics",
    "Requires-Python: >=3.9",
    "Requires-Dist: numpy>=1.21",
    "Description-Content-Type: text/markdown",
    "",
    readme,
  ])


def _digest(data):
  d = base64.urlsafe_b64encode(hashlib.sha256(data).digest())
  return "sha256=" + d.rstrip(b"=").decode()


def build(out_dir):
  """Writes the wheel into out_dir and returns its file name."""
  if not os.path.isfile(LIBRARY):
    sys.exit("d7_build: no {}; build it first: python configure.py, then "
             "make, or msbuild d7.sln on Windows".format(LIBRARY))
  dist_info = "{}-{}.dist-info".format(NAME, version())
  files = []
  for name in sorted(os.listdir(os.path.join(HERE, NAME))):
    if name.endswith(".py"):
      with open(os.path.join(HERE, NAME, name), "rb") as f:
        files.append((NAME + "/" + name, f.read()))
  with open(LIBRARY, "rb") as f:
    files.append((NAME + "/_lib/" + os.path.basename(LIBRARY), f.read()))
  for name in ("LICENSE", "NOTICE"):
    with open(os.path.join(ROOT, name), "rb") as f:
      files.append((dist_info + "/licenses/" + name, f.read()))
  files.append((dist_info + "/METADATA", metadata().encode("utf-8")))
  tag = "py3-none-" + platform_tag()
  files.append((dist_info + "/WHEEL", (
    "Wheel-Version: 1.0\nGenerator: d7_build\nRoot-Is-Purelib: false\n"
    "Tag: {}\n".format(tag)).encode()))
  record = dist_info + "/RECORD"
  lines = ["{},{},{}".format(p, _digest(d), len(d)) for p, d in files]
  files.append((record, ("\n".join(lines + [record + ",,"]) + "\n")
                .encode()))

  wheel = "{}-{}-{}.whl".format(NAME, version(), tag)
  os.makedirs(out_dir, exist_ok=True)
  with zipfile.ZipFile(os.path.join(out_dir, wheel), "w",
                       zipfile.ZIP_DEFLATED) as z:
    for path, data in files:
      info = zipfile.ZipInfo(path, date_time=(1980, 1, 1, 0, 0, 0))
      info.external_attr = (0o755 if path.endswith(
        (".so", ".dylib", ".dll")) else 0o644) << 16
      info.compress_type = zipfile.ZIP_DEFLATED
      z.writestr(info, data)
  return wheel


# PEP 517 ---------------------------------------------------------------

def get_requires_for_build_wheel(config_settings=None):
  return []


def build_wheel(wheel_directory, config_settings=None,
                metadata_directory=None):
  return build(wheel_directory)


if __name__ == "__main__":
  if len(sys.argv) != 2:
    sys.exit("usage: python python/d7_build.py OUT_DIR")
  print(os.path.join(sys.argv[1], build(sys.argv[1])))
