licenses(["notice"])  # BSD
package(default_visibility = ["//visibility:public"])

cc_library(
  name = "fmt",
  includes = ["."],
  srcs = [
    "fmt/format.cc",
    "fmt/os.cc",
  ],
  hdrs = [
    "fmt/args.h",
    "fmt/base.h",
    "fmt/chrono.h",
    "fmt/color.h",
    "fmt/compile.h",
    "fmt/core.h",
    "fmt/format-inl.h",
    "fmt/format.h",
    "fmt/locale.h",
    "fmt/os.h",
    "fmt/ostream.h",
    "fmt/printf.h",
    "fmt/ranges.h",
    "fmt/std.h",
    "fmt/xchar.h",
  ],
  linkstatic = 1,
  copts = ["-w"],
)

