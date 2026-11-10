# D7 — the `.photons` file format

D7 is [Photonscore](https://www.photonscore.de)'s on-disk format for
time-resolved single-photon data (`.photons` files). This repository is the
open-source reference implementation of the native D7 reader/writer for the
"seven" container, without the legacy HDF5 / SQLite / directory-bundle /
QAdata backends.

Every dependency of the C++ library is vendored under
[`third_party/`](third_party). What is not, `configure.py` fetches: protoc
at the exact version of the vendored protobuf runtime, the .NET SDK when
the machine has no suitable one, and the test fixtures on request. Each
file is pinned by version and hash, and goes inside the checkout, nowhere
else.

## Building

### Without a Python of your own

`bootstrap.sh` (Linux, macOS) and `bootstrap.cmd` (Windows) fetch
relocatable CPythons from
[python-build-standalone](https://github.com/astral-sh/python-build-standalone),
pinned by release and SHA-256 in
[`third_party/python/PINS`](third_party/python/PINS), into
`third_party/python/<X.Y>/`. They ask first, as `configure.py` does:

```sh
./bootstrap.sh                 # 3.12; --all for 3.11 to 3.15, --yes for CI
third_party/python/3.12/bin/python configure.py
```

```bat
.\bootstrap.cmd
third_party\python\3.12\python.exe configure.py
```

`. ./env.sh` (Bash, zsh) or `.\env.cmd` then put the checkout's Python
(`D7_PYTHON`, default 3.12) and dotnet first on `PATH`, and keep
dotnet's own caches in `.cache/` too.

### configure.py

Run `configure.py` once per checkout. It needs Python 3.9 or later and
nothing beyond the standard library. It lists what it would download,
with sizes, and asks before fetching anything:

```
configure.py will download, into this checkout only:
  - protoc 3.20.1 (linux-x86_64)        1.7 MB  -> .cache/
  - .NET SDK 10.0.403 (linux-x64)     240.2 MB  -> third_party/dotnet/
  - .NET runtime 8.0.33 (linux-x64)    31.3 MB  -> third_party/dotnet/
                              total   273.2 MB
Proceed? [y/N]
```

| Option | Effect |
| --- | --- |
| `--tests` | Also fetch the test fixtures from [files.photonscore.de](https://files.photonscore.de/data/d7-tests/) into `photonscore/lib/d7/test/data/`. |
| `--fixtures-from DIR_OR_URL` | Take the fixtures from elsewhere; they are checked all the same. |
| `--no-dotnet` | Skip .NET: the native library only. |
| `--yes` | Download without asking, as CI does. |

A machine's own dotnet is used when it has an SDK 10 or later and the
8.0 runtime; otherwise the pinned ones go into `third_party/dotnet/`.
configure.py then generates the protobuf code into `gen/`, and on
Windows sets the projects up for the Visual Studio it finds.

**Linux and macOS** need a C++17 compiler and GNU make:

```sh
python3 configure.py
make -j8
```

**Windows** needs Visual Studio 2019 or later with the C++ desktop
workload:

```bat
python configure.py
msbuild d7.sln -m -p:Configuration=Release -p:Platform=x64
```

`d7.sln` opens in Visual Studio as well. Everything built lands in `out/`
(`out\x64\Release\` on Windows; .NET's under `out/dotnet/`):

| Output | What it is |
| --- | --- |
| `libd7.a` / `libd7.lib` | Static library: D7 together with the protobuf runtime, oneTBB and fmt. |
| `libphotonscore_d7.so` / `.dylib`, `photonscore_d7.dll` | Shared library exporting the `pscr_d7_*` C API ([`c_api.h`](photonscore/lib/d7/c_api.h)). |
| `write_demo` | C++: create a small sample `.photons` file. |
| `list_metadata` | C++: print a file's datasets and attributes. |
| `read_n_events` | C++: read the first N events of a file. |
| `list_metadata_c_api` | `list_metadata` again, over the C API and the shared library. |
| `read_n_events_c_api` | `read_n_events` again, over the C API and the shared library. |

### Run the examples

Write a small sample file, then inspect it and read it back:

```sh
out/write_demo    /tmp/sample.photons
out/list_metadata /tmp/sample.photons
out/read_n_events /tmp/sample.photons 5
```

### Run the tests

The C++ tests (googletest) need no fixtures. `make test` runs them in a
fresh `out/test-run/`; on Windows, `msbuild` builds `d7_test.exe` too:

```bat
mkdir out\test-run && cd out\test-run && ..\x64\Release\d7_test.exe
```

The .NET tests read the fixtures that `configure.py --tests` fetches:

```sh
dotnet test dotnet/Test.D7
```

## Using the library

Link `libd7` (above) and include the umbrella header
[`d7.h`](photonscore/lib/d7/d7.h). A vault is created/opened with `Open`, then
written with `CreateDataset`/`Append` or read with `Read`:

```cpp
#include "photonscore/lib/d7/d7.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace d7 = photonscore::d7;
using photonscore::base::TypeId;

// --- write ---
{
  std::unique_ptr<d7::Vault> vault;
  auto status = d7::Open("scan.photons", d7::OpenOptions::ForWriting(), &vault);
  // ... check status.ok() after every call ...

  d7::DatasetInfo di;
  di.name = "/photons/x";
  di.type = TypeId::kUInt16;
  di.rank = 1;                       // 1-D stream; size[0] grows on append
  status = vault->CreateDataset(di);

  std::vector<uint16_t> x = {10, 11, 13, 16, 20};
  status = vault->Append("/photons/x", x.size(), x.data());
  status = vault->set_attribute("instrument", "LINcam");
  status = vault->Close();           // flushes the global index + epilogue
}

// --- read back ---
{
  std::unique_ptr<d7::Vault> vault;
  auto status = d7::Open("scan.photons", d7::OpenOptions::ForReading(), &vault);

  const d7::DatasetInfo di = vault->dataset_info("/photons/x");
  std::vector<uint16_t> x(di.items());
  status = vault->Read("/photons/x", 0, x.size(), x.data());
}
```

`OpenOptions::ForWriting()` creates a read-write file (backend `"seven"`);
`ForReading()` opens read-only. See
[`examples/write_demo.cc`](examples/write_demo.cc),
[`examples/list_metadata.cc`](examples/list_metadata.cc) and
[`examples/read_n_events.cc`](examples/read_n_events.cc) for complete,
compilable programs. The public surface lives in
[`photonscore/lib/d7`](photonscore/lib/d7): `d7.h`, `vault.h`,
`open_options.h`, `dataset_info.h`.

## Layout

```
photonscore/
  lib/
    base/     small utilities (Status, Slice, DateTime, TypeId)
    fs/       filesystem helpers
    mmap/     memory-mapped file helpers
    path/     path manipulation
    d7/       the D7 reader/writer + C API + seven.proto
  proto/      shared protobuf definitions (base.proto)
examples/     C++ and C-API usage examples (write_demo, list_metadata, read_n_events)
dotnet/
  Photonscore.D7/  the managed reader and writer (net8.0, net10.0)
  Test.D7/         its tests
third_party/
  fmt/        {fmt} formatting library      (MIT)
  oneTBB/     Intel oneAPI TBB              (Apache-2.0)
  protobuf/   Protocol Buffers runtime      (BSD-3-Clause)
  dotnet/     the .NET SDK, when configure.py fetched one (not tracked)
  python/     PINS, and the CPythons bootstrap fetched (not tracked)
msvc/         Visual Studio projects of d7.sln
bootstrap.*   relocatable CPythons into third_party/python/
env.*         the checkout's Python and dotnet on PATH, caches inside
configure.py  downloads, gen/ and the Visual Studio setup
Makefile      the Linux and macOS build
out/          everything built (not tracked)
```

## File format

A `.photons` file is an append-only stream of Protocol Buffers messages laid
over a fixed 16 KB block framing. The message schema is
[`FileEntry`](photonscore/lib/d7/backend/seven/seven.proto); the framing and
codecs are the "seven" backend under
[`photonscore/lib/d7/backend/seven`](photonscore/lib/d7/backend/seven).

### Block framing

The file is a sequence of 16 KB blocks (`kBlockSize = 0x4000`). Each block
starts with a 2-byte header describing the fragment it carries
([`log_format.h`](photonscore/lib/d7/backend/seven/log_format.h)):

| Bits  | Meaning           |
|-------|-------------------|
| 0..13 | Fragment length   |
| 14    | End of fragment   |
| 15    | Begin of fragment |

A serialized message ("fragment") that fits in one block has both the begin
and end bits set. A larger message (up to `kMaxFragment = 16 MB`) is split
across consecutive blocks — the first carries the begin bit, the last the end
bit. When a block has exactly two spare bytes the writer emits an empty begin
marker (`0x8000`); with a single spare byte it pads with `0x00`. This framing
lets a reader land on any block boundary and resynchronise.

### The `FileEntry` stream

Above the framing the file is just a dump of
[`FileEntry`](photonscore/lib/d7/backend/seven/seven.proto) messages, one
after another. `FileEntry` is a `oneof` of four payloads:

- **`Header`** — always the first entry (block 0). Carries the signature
  `"D7 Photons Data"`, the format version, the `index_step`, the page size,
  and a table of contents: one `DatasetInfo` (name, element type, shape) per
  dataset.
- **`Data`** — one encoded page of a single dataset (see
  [Differential encoding](#differential-encoding) below). Pages are ~16 KB
  (`kBytesPerPage`); large appends are encoded page-by-page, in parallel via
  oneTBB.
- **`Index`** — a map from `dataset_id` to the byte `offset` of a run of
  pages, plus any attributes set over that span (see below).
- **`Epilogue`** — the final entry: `global_index_offset` and the signature
  `"End of D7 Photons Data File"`.

Because entries are only ever appended and never rewritten, the format is
**append-only / write-once**: a file can be read while it is still being
written, and once the epilogue is added nothing changes.

### Differential encoding

How a `Data` page stores its samples depends on the dataset's element type.
The relevant fields are the payload of the `Data` message in
[`seven.proto`](photonscore/lib/d7/backend/seven/seven.proto):

| Element type                   | `Data` field(s)                          | Encoding               |
|--------------------------------|------------------------------------------|------------------------|
| `int16/int32`, `uint16/uint32` | `seed` + `integers` (`repeated sint32`)  | delta + zig-zag varint |
| `int64`, `uint64`              | `seed` + `longs` (`repeated sint64`)     | delta + zig-zag varint |
| `int8`, `uint8`                | `raw_bytes` (`bytes`)                     | raw                    |
| `float`, `double`              | `floats` / `doubles`                     | raw                    |

For the integer types the page is stored as first-order differences — `seed`
holds the first sample and each subsequent element is the difference from its
predecessor
([`vault_encoder.cc`](photonscore/lib/d7/backend/seven/vault_encoder.cc)):

```
seed        = x[0]
integers[i] = x[i + 1] - x[i]
```

Since `integers`/`longs` are protobuf `sint32`/`sint64`, they are zig-zag +
varint encoded on the wire, so the small deltas typical of photon
arrival-time and counter streams collapse to one or two bytes each. The
decoder reverses it with a running prefix sum
([`vault_decoder.cc`](photonscore/lib/d7/backend/seven/vault_decoder.cc)):

```
x[0]     = seed
x[i + 1] = integers[i] + x[i]
```

Floating-point and byte datasets carry no diff step — they are copied
verbatim into `floats` / `doubles` / `raw_bytes`.

### Index: partial islands + a global index

Random access needs the byte offset of every `Data` page, but writing one
giant index only at the very end would leave a half-written or crashed file
unreadable. The "seven" backend instead scatters **partial index islands**
through the stream, produced by
[`SevenVaultWriter::FlushIndex`](photonscore/lib/d7/backend/seven/seven_vault_writer.cc):

- As each page is stored, its `{dataset_id, offset}` is appended to an
  in-memory *running* index **and** to a cumulative *global* index.
- Every `index_step` blocks (512 blocks = 8 MB) the running index is
  serialised as an `Index` entry, appended to the stream, and cleared. The
  file thus ends up dotted with small index "islands", each covering the pages
  written since the previous one. Attributes set via `set_attribute` travel in
  the island for their span.
- On `Close` the cumulative global index is written once (tagged with an
  internal marker attribute), the stream is padded to a block boundary, and
  the `Epilogue` — pointing at that global index — is written in a fresh final
  block.

The reader
([`SevenVaultReader::InitIndex`](photonscore/lib/d7/backend/seven/seven_vault_reader.cc))
has two strategies:

1. **Global (fast path)** — read the last block to get the `Epilogue`, seek to
   `global_index_offset`, and replay the `Index` entries found there. Used for
   cleanly closed files.
2. **Scatter / recover** — if there is no valid epilogue (the file is still
   open, or was truncated by a crash) or the caller passes the `recover`
   backend option, the reader hops block-to-block every `index_step` blocks
   and collects the partial index islands to rebuild the page map. With the
   `full` option it then linearly scans any `Data` pages past the last island —
   the tail written since the most recent flush — so even the un-indexed
   remainder of a live file is recoverable.

## Relationship to the Photonscore monorepo

This code was extracted from Photonscore's internal `zoo` monorepo
and pruned to the minimal set of sources
needed to build the library and examples. Everything the code depends on is
vendored under `third_party/`; there are no external, non-vendored
dependencies (in particular, the Boost usage from the monorepo lived only in
now-removed, unused utility code).

## License

Photonscore's own code is licensed under the Apache License 2.0 — see
[`LICENSE`](LICENSE) and [`NOTICE`](NOTICE). Vendored third-party components
under `third_party/` retain their original licenses.

## About

D7 is developed and maintained by [Photonscore GmbH](https://www.photonscore.de),
makers of time-resolved single-photon instrumentation:

- **LINCam** — a position-sensitive single-photon counting system.
- **LINTag** — an ultra-performant time-tagger.

Both produce `.photons` data. Learn more at
[www.photonscore.de](https://www.photonscore.de).
