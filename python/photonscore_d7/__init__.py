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

"""Read and write D7 `.photons` files, Photonscore's single-photon data.

A `.photons` file holds named datasets, typed one-, two- or
three-dimensional arrays that grow along their first axis, and string
attributes. LINCam and LINTag record into it. This package reads and
writes it through the native D7 library, which ships inside the wheel;
data comes and goes as NumPy arrays.

Example:
    Read photon coordinates and an attribute::

        import photonscore_d7 as d7

        with d7.open("scan.photons") as f:
            print(f.attributes["FileCreator"])
            x = f["/photons/x"][:]          # every photon's x
            y = f["/photons/y"][1000:2000]  # a range of them
            for name, ds in f.datasets.items():
                print(name, ds.dtype, ds.shape)

    Write a new file::

        import numpy as np
        import photonscore_d7 as d7

        with d7.create("out.photons") as f:
            x = f.create_dataset("/photons/x", np.uint16)
            x.append(np.arange(100, dtype=np.uint16))
            f.attributes["Comment"] = "a test"
"""

import collections.abc
import ctypes
import threading

import numpy as np

from . import _lib

__all__ = ["D7Error", "Dataset", "File", "create", "open"]

from importlib.metadata import PackageNotFoundError, version as _version

try:
  __version__ = _version("photonscore-d7")
except PackageNotFoundError:
  __version__ = "0+unknown"

_DTYPES = {
  0: np.int8, 1: np.int16, 2: np.int32, 3: np.int64,
  4: np.uint8, 5: np.uint16, 6: np.uint32, 7: np.uint64,
  8: np.float32, 9: np.float64,
}
_TYPE_IDS = {np.dtype(t): i for i, t in _DTYPES.items()}
# The C API counts items in an int.
_MAX_ITEMS = 2**31 - 1


class D7Error(Exception):
  """Raised when the native library reports a failure.

  The message is the library's own, such as a missing file, an unknown
  dataset or a damaged page.
  """


class File:
  """An open `.photons` file. Get one from `open` or `create`.

  A file is a context manager; leaving the `with` block closes it, and a
  file being written gets its index and epilogue only then. Its methods
  may be called from several threads; calls run one at a time.

  Attributes:
      path: The path the file was opened with.
      attributes: The file's attributes, a mutable mapping of `str` to
          `str`. Setting one on a file opened for reading raises
          `D7Error`.
  """

  def __init__(self, path, handle, writable):
    self.path = path
    self._handle = handle
    self._writable = writable
    self._lock = threading.Lock()
    self.attributes = _Attributes(self)

  # -- lifetime --

  def close(self):
    """Closes the file; a file being written is finished first.

    Calling it again does nothing.

    Raises:
        D7Error: If finishing the file failed.
    """
    with self._lock:
      handle, self._handle = self._handle, None
      if handle is None:
        return
      status = _lib.lib.pscr_d7_close(handle)
      message = _lib.lib.pscr_d7_last_error(handle)
      _lib.lib.pscr_d7_free(handle)
    if status != 0:
      raise D7Error(message.decode("utf-8", "replace"))

  @property
  def closed(self):
    """bool: Whether `close` has run."""
    return self._handle is None

  def __enter__(self):
    return self

  def __exit__(self, *exc):
    self.close()

  def __del__(self):
    try:
      self.close()
    except Exception:
      pass

  def __repr__(self):
    state = "closed" if self.closed else (
      "writing" if self._writable else "reading")
    return "<photonscore_d7.File {!r} {}>".format(self.path, state)

  # -- datasets --

  @property
  def datasets(self):
    """dict[str, Dataset]: Every dataset, by name, in name order."""
    with self._lock:
      head = _out(_lib.StringList)
      self._check(_lib.lib.pscr_d7_dataset_names(self._live(), head))
      names = _lib.strings(head)
    return {n.decode(): Dataset(self, n.decode()) for n in sorted(names)}

  def __getitem__(self, name):
    """Returns the dataset `name`.

    Args:
        name: A dataset's full name, such as "/photons/x".

    Returns:
        Dataset: The dataset.

    Raises:
        KeyError: If the file has no such dataset.
    """
    if name not in self.datasets:
      raise KeyError(name)
    return Dataset(self, name)

  def __contains__(self, name):
    return name in self.datasets

  def create_dataset(self, name, dtype, shape=()):
    """Creates an empty dataset in a file opened by `create`.

    Datasets are created before anything is appended to the file.

    Args:
        name: The dataset's full name, such as "/photons/x".
        dtype: Its element type: one of int8 to int64, uint8 to uint64,
            float32 and float64, in any form NumPy accepts.
        shape: The shape of one item, `()` for a stream of values, `(n,)`
            or `(n, m)` for rows of an array.

    Returns:
        Dataset: The new dataset, holding no items.

    Raises:
        ValueError: If `dtype` or `shape` is not one D7 stores.
        D7Error: If the library refused, such as on a duplicate name.
    """
    type_id = _TYPE_IDS.get(np.dtype(dtype))
    if type_id is None:
      raise ValueError("D7 does not store {}".format(np.dtype(dtype)))
    shape = tuple(int(s) for s in shape)
    if len(shape) > 2 or any(s < 1 for s in shape):
      raise ValueError("an item's shape is (), (n,) or (n, m)")
    info = _lib.DatasetInfo()
    info.name = name.encode()
    info.type = type_id
    info.rank = 1 + len(shape)
    for i, s in enumerate(shape):
      info.size[1 + i] = s
    with self._lock:
      self._check(_lib.lib.pscr_d7_create_dataset(self._live(), info))
    return Dataset(self, name)

  # -- internals --

  def _live(self):
    if self._handle is None:
      raise D7Error("the file is closed")
    return self._handle

  def _check(self, status):
    if status != 0:
      message = _lib.lib.pscr_d7_last_error(self._handle)
      raise D7Error(message.decode("utf-8", "replace"))

  def _info(self, name):
    with self._lock:
      ptr = _out(_lib.DatasetInfo)
      self._check(_lib.lib.pscr_d7_dataset_info(
        self._live(), name.encode(), ptr))
      info = ptr.contents
      return (_DTYPES[info.type],
              tuple(int(info.size[i]) for i in range(info.rank)))


class Dataset:
  """One dataset of a `File`: a typed array growing along its first axis.

  Index it as a NumPy array to read: `ds[:]`, `ds[10:20]`, `ds[::2]`,
  `ds[-1]`. Reading always returns a new array.

  Attributes:
      name: The dataset's full name, such as "/photons/x".
  """

  def __init__(self, file, name):
    self._file = file
    self.name = name

  @property
  def dtype(self):
    """numpy.dtype: The element type."""
    return np.dtype(self._file._info(self.name)[0])

  @property
  def shape(self):
    """tuple[int, ...]: The number of items, then an item's shape."""
    return self._file._info(self.name)[1]

  def __len__(self):
    return self.shape[0]

  def __repr__(self):
    return "<photonscore_d7.Dataset {!r} {} {}>".format(
      self.name, self.dtype, self.shape)

  def read(self, offset=0, count=None):
    """Reads `count` items from `offset` on.

    Args:
        offset: The first item.
        count: How many items; all the rest when None.

    Returns:
        numpy.ndarray: Shape `(count,)` plus an item's shape, of the
        dataset's dtype.

    Raises:
        ValueError: If the range is not inside the dataset.
        D7Error: If the library failed, such as on a damaged page.
    """
    dtype, shape = self._file._info(self.name)
    if count is None:
      count = shape[0] - offset
    if offset < 0 or count < 0 or offset + count > shape[0]:
      raise ValueError("items [{}, {}) are outside {} of {}".format(
        offset, offset + count, self.name, shape[0]))
    out = np.empty((count,) + shape[1:], dtype=dtype)
    done = 0
    with self._file._lock:
      handle = self._file._live()
      name = self.name.encode()
      while done < count:
        n = min(count - done, _MAX_ITEMS)
        self._file._check(_lib.lib.pscr_d7_read(
          handle, name, offset + done, n, out[done:].ctypes.data))
        done += n
    return out

  def __getitem__(self, key):
    """Reads like NumPy indexing on the first axis.

    Args:
        key: An int or a slice.

    Returns:
        numpy.ndarray: The items; for an int, the one item (a NumPy
        scalar for a stream).
    """
    n = len(self)
    if isinstance(key, slice):
      start, stop, step = key.indices(n)
      if step == 1:
        return self.read(start, max(stop - start, 0))
      picked = np.arange(start, stop, step)
      if len(picked) == 0:
        return self.read(0, 0)
      lo = int(picked.min())
      return self.read(lo, int(picked.max()) + 1 - lo)[picked - lo]
    if isinstance(key, (int, np.integer)):
      i = int(key) + n if key < 0 else int(key)
      if not 0 <= i < n:
        raise IndexError("item {} of {}".format(key, n))
      return self.read(i, 1)[0]
    raise TypeError("index a Dataset with an int or a slice")

  def append(self, data):
    """Appends items to a dataset of a file opened by `create`.

    Args:
        data: Array-like of items: shape `(k,)` plus an item's shape,
            converted to the dataset's dtype.

    Raises:
        ValueError: If the items have the wrong shape.
        D7Error: If the library failed.
    """
    dtype, shape = self._file._info(self.name)
    a = np.ascontiguousarray(data, dtype=dtype)
    if a.ndim == len(shape) - 1:
      a = a.reshape((1,) + a.shape)
    if a.shape[1:] != shape[1:]:
      raise ValueError("items of shape {} for {} of item shape {}".format(
        a.shape[1:], self.name, shape[1:]))
    done = 0
    with self._file._lock:
      handle = self._file._live()
      name = self.name.encode()
      while done < len(a):
        n = min(len(a) - done, _MAX_ITEMS)
        self._file._check(_lib.lib.pscr_d7_append(
          handle, name, n, a[done:].ctypes.data))
        done += n


class _Attributes(collections.abc.MutableMapping):
  """A file's attributes, read from the file on every access."""

  def __init__(self, file):
    self._file = file

  def _names(self):
    with self._file._lock:
      head = _out(_lib.StringList)
      self._file._check(_lib.lib.pscr_d7_attribute_names(
        self._file._live(), head))
      return [n.decode() for n in _lib.strings(head)]

  def __getitem__(self, key):
    if key not in self._names():
      raise KeyError(key)
    with self._file._lock:
      size = ctypes.c_int()
      value = _out(ctypes.c_char)
      self._file._check(_lib.lib.pscr_d7_attribute(
        self._file._live(), key.encode(), size, value))
      raw = ctypes.string_at(value, size.value)
    return raw.decode("utf-8", "replace")

  def __setitem__(self, key, value):
    raw = value if isinstance(value, bytes) else str(value).encode()
    with self._file._lock:
      self._file._check(_lib.lib.pscr_d7_set_attribute(
        self._file._live(), key.encode(), len(raw), raw))

  def __delitem__(self, key):
    raise D7Error("D7 attributes cannot be removed")

  def __iter__(self):
    return iter(self._names())

  def __len__(self):
    return len(self._names())

  def __repr__(self):
    return repr(dict(self))


def _out(t):
  """A NULL pointer to `t`; ctypes passes it by reference."""
  return ctypes.POINTER(t)()


def _open(path, read_only, create_if_missing, fail_if_exists, recover,
          read_threads):
  options = _lib.OpenOptions()
  options.fail_if_exists = int(fail_if_exists)
  options.create_if_missing = int(create_if_missing)
  options.read_only = int(read_only)
  options.read_threads = int(read_threads)
  options.backend = b"seven"
  options.backend_options = b"recover full" if recover else None
  handle = ctypes.c_void_p()
  status = _lib.lib.pscr_d7_open(str(path).encode(), options, handle)
  if status != 0:
    # Not freed: the C API's free closes a vault a failed open never made.
    message = _lib.lib.pscr_d7_last_error(handle)
    raise D7Error(message.decode("utf-8", "replace"))
  return File(str(path), handle.value, writable=not read_only)


def open(path, recover=False, read_threads=4):
  """Opens a `.photons` file for reading.

  Args:
      path: The file.
      recover: Rebuild the index by scanning the file, for one that was
          never closed: a crashed or still-running recording, or a
          truncated copy.
      read_threads: How many threads a read may use.

  Returns:
      File: The open file; use it in a `with` block.

  Raises:
      D7Error: If the file is missing or is not a D7 file.

  Example:
      >>> with photonscore_d7.open("scan.photons") as f:
      ...     x = f["/photons/x"][:]
  """
  return _open(path, True, False, False, recover, read_threads)


def create(path, overwrite=False):
  """Creates a new `.photons` file for writing.

  Create its datasets first, then append to them and set attributes;
  closing the file writes its index.

  Args:
      path: The file to create.
      overwrite: Replace an existing file instead of raising.

  Returns:
      File: The new file; use it in a `with` block.

  Raises:
      D7Error: If the file exists and `overwrite` is False, or it cannot
          be created.
  """
  return _open(path, False, True, not overwrite, False, 4)
