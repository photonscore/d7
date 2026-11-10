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
using System.Security.Cryptography;

namespace Test.D7;

// One call to a writer, replayed
internal abstract record Step;

internal sealed record CreateStep(string Name, Type Type, int[] Dims) : Step;

// Values [From, From + Count) of Data
internal sealed record AppendStep(string Name, Array Data, int From, int Count)
    : Step;

internal sealed record AttributeStep(string Key, string Value) : Step;

internal static class Steps {
  public static void Managed(string path, IEnumerable<Step> steps,
                             ParallelOptions? options = null,
                             Action<D7FileWriter>? setup = null) {
    using var w = new D7FileWriter(path);
    setup?.Invoke(w);
    foreach (var step in steps)
      Replay(w, step, options);
  }

  public static void Replay(D7FileWriter w, Step step,
                            ParallelOptions? options) {
    switch (step) {
      case CreateStep c:
        w.CreateDataset(c.Name, c.Type, c.Dims);
        break;
      case AppendStep a:
        Invoke(nameof(ManagedAs), a.Data,
               [w, a.Name, a.Data, a.From, a.Count, options]);
        break;
      case AttributeStep a:
        w[a.Key] = a.Value;
        break;
    }
  }

  // The sets created, then appended at random, 1 to 40,000 items at a time
  public static List<Step> Interleaved(
      (string Name, Array Data, int[] Dims)[] sets, Random rng) {
    var steps = new List<Step>();
    foreach (var (name, data, dims) in sets) {
      steps.Add(new CreateStep(name, data.GetType().GetElementType()!, dims));
    }
    int[] elems = [.. sets.Select(s => s.Dims.Aggregate(1, (a, b) => a * b))];
    var at = new int[sets.Length];
    while (true) {
      var open = Enumerable.Range(0, sets.Length)
        .Where(s => at[s] < sets[s].Data.Length).ToArray();
      if (open.Length == 0)
        break;
      int s = open[rng.Next(open.Length)];
      int items = rng.Next(3) switch {
        0 => rng.Next(1, 8),
        1 => rng.Next(1, 3000),
        _ => rng.Next(1, 40_000),
      };
      int count = Math.Min(sets[s].Data.Length - at[s], items * elems[s]);
      steps.Add(new AppendStep(sets[s].Name, sets[s].Data, at[s], count));
      at[s] += count;
    }
    return steps;
  }

  public static string Sha(string path) {
    using var file = File.OpenRead(path);
    return Convert.ToHexStringLower(SHA256.HashData(file));
  }

  static void ManagedAs<T>(D7FileWriter w, string name, Array data, int from,
                           int count, ParallelOptions? options)
      where T : unmanaged =>
    w.Append(new ReadOnlySpan<T>((T[])data, from, count), name, options);

  static void Invoke(string method, Array data, object?[] args) {
    try {
      typeof(Steps).GetMethod(method, BindingFlags.NonPublic |
                                      BindingFlags.Static)!
        .MakeGenericMethod(data.GetType().GetElementType()!)
        .Invoke(null, args);
    } catch (TargetInvocationException e) when (e.InnerException != null) {
      System.Runtime.ExceptionServices.ExceptionDispatchInfo
        .Capture(e.InnerException).Throw();
    }
  }
}
