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

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  int err;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <filename.photons>\n", argv[0]);
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

  PscrD7StringList *list;
  err = pscr_d7_attribute_names(f, &list);
  if (err) {
    fprintf(stderr, "pscr_d7_attribute_name fails: '%s'\n",
            pscr_d7_last_error(f));
    return 1;
  }

  printf("ATTRIBUTES:\n");
  while (list->next) {
    int attr_len;
    const char *attr_value;
    err = pscr_d7_attribute(f, list->value, &attr_len, &attr_value);
    if (err) {
      fprintf(stderr, "pscr_d7_attribute fails: '%s'\n", pscr_d7_last_error(f));
      return 1;
    }
    printf("  %s: %s\n", list->value, attr_value);
    list = list->next;
  }
  printf("\n");

  err = pscr_d7_dataset_names(f, &list);
  if (err) {
    fprintf(stderr, "pscr_d7_dataset_names fails: '%s'\n",
            pscr_d7_last_error(f));
    return 1;
  }

  printf("DATASETS:\n");
  while (list->next) {
    PscrD7DatasetInfo *di;
    err = pscr_d7_dataset_info(f, list->value, &di);
    if (err) {
      fprintf(stderr, "pscr_d7_dataset_info fails: '%s'\n",
              pscr_d7_last_error(f));
      return 1;
    }
    printf("  %s: ", di->name);
    switch (di->type) {
      case PSCR_D7_T_INT8:
        printf("int8");
        break;
      case PSCR_D7_T_INT16:
        printf("int16");
        break;
      case PSCR_D7_T_INT32:
        printf("int32");
        break;
      case PSCR_D7_T_INT64:
        printf("int64");
        break;
      case PSCR_D7_T_UINT8:
        printf("uint8");
        break;
      case PSCR_D7_T_UINT16:
        printf("uint16");
        break;
      case PSCR_D7_T_UINT32:
        printf("uint32");
        break;
      case PSCR_D7_T_UINT64:
        printf("uint64");
        break;
      case PSCR_D7_T_SINGLE:
        printf("float");
        break;
      case PSCR_D7_T_DOUBLE:
        printf("double");
        break;
      default:
        printf("UNKNOWN");
        break;
    }
    printf("[");
    for (int i = 0; i < di->rank; i++) {
      printf("%lld", (long long)di->size[i]);
      if (i < (di->rank - 1)) {
        printf(", ");
      }
    }
    printf("]\n");
    list = list->next;
  }
  printf("\n");

  err = pscr_d7_close(f);
  if (err) {
    fprintf(stderr, "pscr_d7_close fails: '%s'\n", pscr_d7_last_error(f));
    return 1;
  }

  pscr_d7_free(f);

  return 0;
}
