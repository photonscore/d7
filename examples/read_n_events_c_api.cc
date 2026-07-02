/*
    Copyright (c) 2026 Photonscore GmbH

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "photonscore/lib/d7/c_api.h"

#include <cstdint>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  int err;

  if (argc != 3) {
    fprintf(stderr, "usage: %s <filename.photons> <n>\n", argv[0]);
    return 1;
  }

  PscrD7 *f;
  PscrD7OpenOptions oo;
  memset(&oo, 0, sizeof(oo));

  oo.read_only = 1;
  err = pscr_d7_open(argv[1], &oo, &f);
  if (err) {
    fprintf(stderr, "pscr_d7_open fails: '%s'\n", pscr_d7_last_error(f));
    return 1;
  }

  size_t n = atoi(argv[2]);
  std::vector<uint16_t> x, y, dt;
  x.resize(n);
  y.resize(n);
  dt.resize(n);

  err = pscr_d7_read(f, "/photons/x", 0, n, x.data());
  if (err) {
    fprintf(stderr, "fail reading X: '%s'\n", pscr_d7_last_error(f));
    return 1;
  }

  err = pscr_d7_read(f, "/photons/y", 0, n, y.data());
  if (err) {
    fprintf(stderr, "fail reading Y: '%s'\n", pscr_d7_last_error(f));
    return 1;
  }

  err = pscr_d7_read(f, "/photons/dt", 0, n, dt.data());
  if (err) {
    fprintf(stderr, "fail reading DT: '%s'\n", pscr_d7_last_error(f));
    return 1;
  }

  printf("X\tY\tDT\n");
  for (size_t i = 0; i < n; i++) {
    printf("%d\t%d\t%d\n", x[i], y[i], dt[i]);
  }

  err = pscr_d7_close(f);
  if (err) {
    fprintf(stderr, "pscr_d7_close fails: '%s'\n", pscr_d7_last_error(f));
    return 1;
  }

  pscr_d7_free(f);

  return 0;
}
