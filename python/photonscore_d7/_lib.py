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

"""The C API of photonscore_d7's shared library, through ctypes."""

import ctypes
import os
import sys

if sys.platform == "win32":
  _NAMES = ["photonscore_d7.dll"]
  _BUILT = os.path.join("out", "x64", "Release")
elif sys.platform == "darwin":
  _NAMES = ["libphotonscore_d7.dylib"]
  _BUILT = "out"
else:
  _NAMES = ["libphotonscore_d7.so"]
  _BUILT = "out"


class OpenOptions(ctypes.Structure):
  _fields_ = [
    ("fail_if_exists", ctypes.c_int),
    ("create_if_missing", ctypes.c_int),
    ("read_only", ctypes.c_int),
    ("read_threads", ctypes.c_int),
    ("backend", ctypes.c_char_p),
    ("backend_options", ctypes.c_char_p),
  ]


class DatasetInfo(ctypes.Structure):
  _fields_ = [
    ("name", ctypes.c_char_p),
    ("type", ctypes.c_int),
    ("rank", ctypes.c_int),
    ("size", ctypes.c_int64 * 3),
  ]


class StringList(ctypes.Structure):
  pass


StringList._fields_ = [
  ("next", ctypes.POINTER(StringList)),
  ("value_length", ctypes.c_int),
  ("value", ctypes.POINTER(ctypes.c_char)),
]


def _candidates():
  """The wheel's own copy first, then an override, then a checkout's."""
  here = os.path.dirname(os.path.abspath(__file__))
  for name in _NAMES:
    yield os.path.join(here, "_lib", name)
  override = os.environ.get("PHOTONSCORE_D7_LIBRARY")
  if override:
    yield override
  d = here
  while True:
    if os.path.isfile(os.path.join(d, "configure.py")):
      for name in _NAMES:
        yield os.path.join(d, _BUILT, name)
      break
    parent = os.path.dirname(d)
    if parent == d:
      break
    d = parent


def _load():
  tried = []
  for path in _candidates():
    if os.path.isfile(path):
      return ctypes.CDLL(path), path
    tried.append(path)
  raise ImportError("photonscore_d7: no shared library; looked at "
                    + ", ".join(tried))


lib, path = _load()

_P = ctypes.c_void_p
_S = ctypes.c_char_p
_I = ctypes.c_int


def _sig(name, restype, *argtypes):
  f = getattr(lib, name)
  f.restype = restype
  f.argtypes = list(argtypes)


_sig("pscr_d7_open", _I, _S, ctypes.POINTER(OpenOptions),
     ctypes.POINTER(_P))
_sig("pscr_d7_close", _I, _P)
_sig("pscr_d7_free", None, _P)
_sig("pscr_d7_last_error", _S, _P)
_sig("pscr_d7_read", _I, _P, _S, ctypes.c_int64, _I, _P)
_sig("pscr_d7_append", _I, _P, _S, _I, _P)
_sig("pscr_d7_create_dataset", _I, _P, ctypes.POINTER(DatasetInfo))
_sig("pscr_d7_dataset_names", _I, _P,
     ctypes.POINTER(ctypes.POINTER(StringList)))
_sig("pscr_d7_dataset_info", _I, _P, _S,
     ctypes.POINTER(ctypes.POINTER(DatasetInfo)))
_sig("pscr_d7_attribute_names", _I, _P,
     ctypes.POINTER(ctypes.POINTER(StringList)))
_sig("pscr_d7_attribute", _I, _P, _S, ctypes.POINTER(_I),
     ctypes.POINTER(ctypes.POINTER(ctypes.c_char)))
_sig("pscr_d7_set_attribute", _I, _P, _S, _I, _S)


def strings(head):
  """A C string list as Python bytes, copied before the next call reuses
  the library's buffer."""
  out = []
  node = head
  while node and node.contents.value:
    item = node.contents
    out.append(ctypes.string_at(item.value, item.value_length))
    node = item.next
  return out
