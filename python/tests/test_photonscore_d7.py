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

import hashlib
import os
import shutil

import numpy as np
import pytest

import photonscore_d7 as d7

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
  os.path.abspath(__file__))))
D7_PHOTONS = os.path.join(ROOT, "photonscore", "lib", "d7", "test", "data",
                          "d7.photons")

# d7.photons as the .NET tests pin it (dotnet/Test.D7/SevenReadTest.cs)
LAYOUT = [
  ("/auxiliary/time", "uint64", 0),
  ("/auxiliary/value", "uint16", 0),
  ("/photons/a", "uint16", 0),
  ("/photons/aat", "uint64", 0),
  ("/photons/dt", "uint16", 625300),
  ("/photons/ms", "uint64", 141324),
  ("/photons/q", "uint16", 0),
  ("/photons/x", "uint16", 625300),
  ("/photons/y", "uint16", 625300),
  ("/status/auxiliary", "uint16", 0),
  ("/status/daq_counts", "uint64", 0),
  ("/status/mcp_counts", "uint64", 0),
  ("/status/t", "uint64", 0),
  ("/status/temperature", "float64", 0),
  ("/status/voltage", "float64", 0),
]
HASHES = {
  "/photons/dt":
    "317c4d779c8863c1eb4e0b6d6ee691dd74f3ce73ad73aca2094dcd5162ba01da",
  "/photons/ms":
    "8f79d01e63709a0ec4ab8c6fea15547f5def997ebb85b466defcc749c9b9a3af",
  "/photons/x":
    "af4709a0e927a25b1060cc4fe0d9920cd35e36e000bff5ce521cd1e00ada4c9d",
  "/photons/y":
    "dea4eb8ef1e7ec9d24ef6d481a19ad6a216ef00db191342fec3b3772cf89d864",
}
ATTRIBUTES = {
  "/photons/DetectorGuid": "{961a443c-697d-426b-b991-2abe8485695c}",
  "/photons/IsNewFile": "0",
  "/photons/PositionBits": "12",
  "/photons/TacBias": "4095",
  "/photons/TacBits": "12",
  "/photons/TacChannel": "11.65",
  "/photons/TimerFrequency": "100000000",
  "Comment": "<comment>",
  "Created": "2019-02-27 14:01:33.112",
  "FileCreator": "LnTCapture Version 2017.1.0.0",
  "FileGuid": "{fcd6107a-b0e5-4a95-ad15-764b2c731730}",
  "FileRecorderVersion": "2017.0.1 (78a4205d)",
}
DTYPES = ["int8", "int16", "int32", "int64", "uint8", "uint16", "uint32",
          "uint64", "float32", "float64"]


def sha(a):
  return hashlib.sha256(np.ascontiguousarray(a).tobytes()).hexdigest()


def test_d7_photons_reads_as_pinned():
  with d7.open(D7_PHOTONS) as f:
    layout = [(n, str(ds.dtype), ds.shape[0])
              for n, ds in f.datasets.items()]
    assert layout == LAYOUT
    assert dict(f.attributes) == ATTRIBUTES
    for name, digest in HASHES.items():
      assert sha(f[name][:]) == digest


def test_ranges_and_indexing_match_the_whole():
  with d7.open(D7_PHOTONS) as f:
    ds = f["/photons/x"]
    whole = ds[:]
    n = len(whole)
    rng = np.random.default_rng(11)
    for _ in range(50):
      a, b = sorted(rng.integers(0, n, 2))
      assert np.array_equal(ds.read(int(a), int(b - a)), whole[a:b])
    for key in [slice(None, None, 3), slice(-10, None), slice(10, 0, -3),
                slice(n - 1, None, -7), slice(5, 5), slice(0, 16385)]:
      assert np.array_equal(ds[key], whole[key])
    assert ds[0] == whole[0] and ds[-1] == whole[-1]
    with pytest.raises(IndexError):
      ds[n]
    with pytest.raises(ValueError):
      ds.read(n - 1, 2)


def test_written_files_read_back(tmp_path):
  path = str(tmp_path / "written.photons")
  rng = np.random.default_rng(5)
  data = {}
  for i, t in enumerate(DTYPES):
    shape = [(), (3,), (2, 4)][i % 3]
    if np.dtype(t).kind == "f":
      a = rng.standard_normal((70001,) + shape).astype(t)
    else:
      info = np.iinfo(t)
      a = rng.integers(info.min, info.max, (70001,) + shape, dtype=t,
                       endpoint=True)
    data["/t/{}/{}".format(t, len(shape))] = a
  with d7.create(path) as f:
    # D7 takes every dataset before the first append
    for name, a in data.items():
      f.create_dataset(name, a.dtype, a.shape[1:])
    f.create_dataset("/t/empty", np.uint32)
    for lo in range(0, 70001, 30000):
      for name, a in data.items():
        f[name].append(a[lo:lo + 30000])
    f.attributes["Comment"] = "grün, 光子, 12 µm"
    f.attributes["Long"] = "x" * 5000
  with d7.open(path) as f:
    assert sorted(f.datasets) == sorted(list(data) + ["/t/empty"])
    for name, a in data.items():
      assert f[name].dtype == a.dtype
      assert f[name].shape == a.shape
      assert np.array_equal(f[name][:], a)
    assert len(f["/t/empty"]) == 0
    assert f.attributes["Comment"] == "grün, 光子, 12 µm"
    assert f.attributes["Long"] == "x" * 5000


def test_a_file_never_closed_is_recovered(tmp_path):
  path = str(tmp_path / "whole.photons")
  x = np.arange(2_000_000, dtype=np.uint32) * 7
  with d7.create(path) as f:
    f.create_dataset("/photons/x", np.uint32).append(x)
  cut = str(tmp_path / "cut.photons")
  shutil.copyfile(path, cut)
  with open(cut, "r+b") as raw:
    raw.truncate(os.path.getsize(cut) - 16384)
  with d7.open(cut, recover=True) as f:
    got = f["/photons/x"][:]
  assert len(got) > 0
  assert np.array_equal(got, x[:len(got)])


def test_create_keeps_an_existing_file_unless_told(tmp_path):
  path = str(tmp_path / "exists.photons")
  with d7.create(path) as f:
    f.create_dataset("/a", np.int16).append([1, 2, 3])
  with pytest.raises(d7.D7Error):
    d7.create(path)
  with d7.create(path, overwrite=True) as f:
    f.create_dataset("/b", np.int16)
  with d7.open(path) as f:
    assert list(f.datasets) == ["/b"]


def test_mistakes_raise(tmp_path):
  with pytest.raises(d7.D7Error):
    d7.open(str(tmp_path / "none.photons"))
  path = str(tmp_path / "m.photons")
  f = d7.create(path)
  ds = f.create_dataset("/rows", np.float32, (3,))
  with pytest.raises(ValueError):
    ds.append(np.zeros((2, 4)))
  with pytest.raises(ValueError):
    f.create_dataset("/bool", np.bool_)
  f.close()
  f.close()
  with pytest.raises(d7.D7Error):
    f.datasets
  with d7.open(path) as g:
    with pytest.raises(KeyError):
      g["/nothing"]
    with pytest.raises(KeyError):
      g.attributes["nothing"]
