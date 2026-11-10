// Copyright (c) 2026 Photonscore GmbH
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

using Photonscore.D7;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

namespace Test.D7;

// A seven file: every element type, ranks 2 and 3, an empty dataset and
// attributes, interleaved appends past three running indexes. Written once
// per test class, deleted after.
public sealed class WrittenFile : IDisposable {
  public readonly string Dir;
  public readonly string Path;
  public readonly (string Name, Array Data, int[] Dims)[] Sets;
  public readonly Dictionary<string, string> Attributes = new() {
    ["Comment"] = "grün, 光子, 12 µm",
    ["Empty"] = "",
    ["Long"] = new string('x', 5000),
    ["Created"] = "2026-09-26 12:00:00.000",
  };

  public WrittenFile() {
    Dir = Directory.CreateTempSubdirectory("d7-seven-").FullName;
    Path = System.IO.Path.Combine(Dir, "written.photons");
    Sets = Generate(new Random(5), 900_000);
    using var w = new D7FileWriter(Path);
    foreach (var (name, data, dims) in Sets) {
      w.CreateDataset(name, data.GetType().GetElementType()!, dims);
    }
    w.CreateDataset<uint>("/t/empty");
    w["Comment"] = Attributes["Comment"];
    w["Empty"] = Attributes["Empty"];
    var rng = new Random(7);
    var at = new int[Sets.Length];
    while (true) {
      var open = Enumerable.Range(0, Sets.Length)
        .Where(s => at[s] < Items(s)).ToArray();
      if (open.Length == 0)
        break;
      int s = open[rng.Next(open.Length)];
      int take = Math.Min(Items(s) - at[s], rng.Next(3) switch {
        0 => rng.Next(1, 8),
        1 => rng.Next(1, 3000),
        _ => rng.Next(1, 40_000),
      });
      Append(w, s, at[s], take);
      at[s] += take;
    }
    w["Long"] = Attributes["Long"];
    w["Created"] = Attributes["Created"];
  }

  public void Dispose() => TempDir.Delete(Dir);

  public int Items(int s) => Sets[s].Data.Length / ItemElems(s);

  int ItemElems(int s) => Sets[s].Dims.Aggregate(1, (a, b) => a * b);

  void Append(D7FileWriter w, int s, int from, int count) {
    int e = ItemElems(s);
    const BindingFlags Private = BindingFlags.NonPublic | BindingFlags.Static;
    typeof(WrittenFile).GetMethod(nameof(AppendAs), Private)!
      .MakeGenericMethod(Sets[s].Data.GetType().GetElementType()!)
      .Invoke(null, [w, Sets[s].Name, Sets[s].Data, from * e, count * e]);
  }

  static void AppendAs<T>(D7FileWriter w, string name, Array data, int from,
                          int count) where T : unmanaged =>
    w.Append(new ReadOnlySpan<T>((T[])data, from, count), name);

  // Random walks with jumps: varints of every length the encoder makes
  internal static (string, Array, int[])[] Generate(Random rng, int n) {
    var i8 = new sbyte[n];
    var u8 = new byte[n];
    var i16 = new short[n];
    var u16 = new ushort[n];
    var i32 = new int[n];
    var u32 = new uint[n];
    var i64 = new long[n];
    var u64 = new ulong[n];
    var f32 = new float[n];
    var f64 = new double[n];
    rng.NextBytes(u8);
    rng.NextBytes(MemoryMarshal.AsBytes(i8.AsSpan()));
    long walk = 0;
    for (int i = 0; i < n; i++) {
      bool jump = rng.Next(50) == 0;
      int prev = Math.Max(0, i - 1);
      i16[i] = (short)(jump ? rng.Next(short.MinValue, short.MaxValue)
                            : i16[prev] + rng.Next(-300, 300));
      u16[i] = (ushort)(jump ? rng.Next(0, 65536)
                             : u16[prev] + rng.Next(-300, 300));
      i32[i] = jump ? rng.Next(int.MinValue, int.MaxValue)
                    : i32[prev] + rng.Next(-100_000, 100_000);
      u32[i] = jump ? (uint)rng.NextInt64(0, uint.MaxValue)
                    : u32[prev] + (uint)rng.Next(0, 1_000_000);
      walk += jump ? rng.NextInt64() : rng.Next(0, 20_000);
      i64[i] = jump ? -walk : walk;
      u64[i] = (ulong)walk * 3;
      f32[i] = (float)(rng.NextDouble() * 1e3);
      f64[i] = rng.NextDouble() * 1e9;
    }
    var raw = new byte[n / 10 * 32];
    rng.NextBytes(raw);
    var xyz = new ushort[n / 3 * 3];
    for (int i = 0; i < xyz.Length; i++)
      xyz[i] = (ushort)rng.Next(4096);
    var cube = new short[n / 20 * 20];
    for (int i = 0; i < cube.Length; i++)
      cube[i] = (short)rng.Next(-99, 99);
    return [
      ("/t/i8", i8, []), ("/t/u8", u8, []), ("/t/i16", i16, []),
      ("/t/u16", u16, []), ("/t/i32", i32, []), ("/t/u32", u32, []),
      ("/t/i64", i64, []), ("/t/u64", u64, []), ("/t/f32", f32, []),
      ("/t/f64", f64, []), ("/t/raw", raw, [32]), ("/t/xyz", xyz, [3]),
      ("/t/cube", cube, [4, 5]),
    ];
  }
}

// Whole datasets and ranges as bytes, by the element type the dataset
// declares
internal static class SevenRead {
  public static byte[] Managed(D7FileReader r, string name, long offset,
                               int items, ParallelOptions? options = null) {
    var di = r.Datasets[name];
    return Invoke(nameof(ManagedAs), di.Type,
                  [r, name, offset, items, (int)(items * di.ItemSize),
                   options]);
  }

  static byte[] Invoke(string method, Type type, object?[] args) =>
    (byte[])typeof(SevenRead)
      .GetMethod(method, BindingFlags.NonPublic | BindingFlags.Static)!
      .MakeGenericMethod(type).Invoke(null, args)!;

  static byte[] ManagedAs<T>(D7FileReader r, string name, long offset,
                             int items, int elems, ParallelOptions? options)
      where T : unmanaged {
    var a = new T[elems];
    r.Read(a, name, offset, items, options);
    return MemoryMarshal.AsBytes(a.AsSpan()).ToArray();
  }

  // What a reader holds as one hash: its datasets in name order, each with
  // its type, shape and bytes, then its attributes
  public static string Digest(D7FileReader r) {
    using var h = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
    foreach (var name in r.Datasets.Keys.Order(StringComparer.Ordinal)) {
      var di = r.Datasets[name];
      h.AppendData(Encoding.UTF8.GetBytes(
        $"{name} {di.Type.Name} {string.Join(' ', di.Dims)}\n"));
      if (di.Items > 0)
        h.AppendData(Managed(r, name, 0, (int)di.Items));
    }
    foreach (var (key, value) in
             r.Attributes.OrderBy(a => a.Key, StringComparer.Ordinal)) {
      h.AppendData(Encoding.UTF8.GetBytes($"{key}={value}\n"));
    }
    return Convert.ToHexStringLower(h.GetHashAndReset());
  }

  public static string Sha(byte[] bytes) =>
    Convert.ToHexStringLower(SHA256.HashData(bytes));

  public static byte[] Bytes(Array data) {
    var h = GCHandle.Alloc(data, GCHandleType.Pinned);
    try {
      int n = data.Length * Marshal.SizeOf(data.GetType().GetElementType()!);
      var bytes = new byte[n];
      Marshal.Copy(h.AddrOfPinnedObject(), bytes, 0, n);
      return bytes;
    } finally {
      h.Free();
    }
  }
}
