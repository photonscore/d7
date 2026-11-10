# Photonscore.D7

Reads and writes D7 `.photons` files, [Photonscore](https://www.photonscore.de)'s
format for time-resolved single-photon data, as recorded by LINCam and
LINTag. Managed code only, for .NET 8 and .NET 10: no native library and
no protobuf. What it writes is the native library's file byte for byte,
and it reads every file the native library wrote.

```csharp
using Photonscore.D7;

// Read: three columns in one pass, on four of the caller's threads
using (var r = new D7FileReader("scan.photons")) {
  int n = (int)r.Datasets["/photons/x"].Items;
  var x = new ushort[n];
  var y = new ushort[n];
  var dt = new ushort[n];
  r.Read([D7Column.Of("/photons/x", x), D7Column.Of("/photons/y", y),
          D7Column.Of("/photons/dt", dt)], 0, n,
         new ParallelOptions { MaxDegreeOfParallelism = 4 });
  Console.WriteLine(r["FileCreator"]);
}

// Write: datasets first, then appends and attributes; Dispose closes
using (var w = new D7FileWriter("out.photons")) {
  w.CreateDataset<ushort>("/photons/x");
  w["Comment"] = "...";
  w.Append(new ushort[] { 10, 11, 13 }, "/photons/x");
}
```

A file that was never closed, by a crash or because it is still being
recorded, opens all the same: the reader recovers it from its running
indexes, and `Recovered` says so.

The format, the native C++ library and its C API live at
[github.com/photonscore/d7](https://github.com/photonscore/d7).
Apache-2.0.
