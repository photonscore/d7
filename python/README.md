# photonscore-d7

Reads and writes D7 `.photons` files, [Photonscore](https://www.photonscore.de)'s
format for time-resolved single-photon data, as recorded by LINCam and
LINTag. It runs the native D7 library, which ships inside the wheel, and
hands data over as NumPy arrays. One wheel per platform serves every
Python 3: Windows x64, macOS on Apple silicon and Linux x86_64.

```sh
pip install photonscore-d7
```

```python
import numpy as np
import photonscore_d7 as d7

# Read
with d7.open("scan.photons") as f:
    print(f.attributes["FileCreator"])
    for name, ds in f.datasets.items():
        print(name, ds.dtype, ds.shape)
    x = f["/photons/x"][:]          # every photon's x
    y = f["/photons/y"][1000:2000]  # a range of them

# Write: datasets first, then appends and attributes
with d7.create("out.photons") as f:
    x = f.create_dataset("/photons/x", np.uint16)
    x.append(np.arange(100, dtype=np.uint16))
    f.attributes["Comment"] = "a test"
```

A file that was never closed, by a crash or because it is still being
recorded, opens with `d7.open(path, recover=True)`.

The format, the C++ library, its C API and the .NET package live at
[github.com/photonscore/d7](https://github.com/photonscore/d7).
Apache-2.0.
