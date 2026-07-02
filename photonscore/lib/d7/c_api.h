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

#ifndef PHOTONSCORE_FILE_C_API_H
#define PHOTONSCORE_FILE_C_API_H

#include <stdint.h>

#define PSCR_D7_T_INT8 0
#define PSCR_D7_T_INT16 1
#define PSCR_D7_T_INT32 2
#define PSCR_D7_T_INT64 3

#define PSCR_D7_T_UINT8 4
#define PSCR_D7_T_UINT16 5
#define PSCR_D7_T_UINT32 6
#define PSCR_D7_T_UINT64 7

#define PSCR_D7_T_SINGLE 8
#define PSCR_D7_T_DOUBLE 9

#define PSCR_D7_T_UUID 14

#ifdef __cplusplus
extern "C" {
#endif

#if (defined PSCR_D7_EXPORT)
#ifdef _WIN32
#define PSCR_D7_API __declspec(dllexport)
#else
#define PSCR_D7_API __attribute__((visibility("default")))
#endif

#elif (defined PSCR_D7_IMPORT)
#ifdef _WIN32
#define PSCR_D7_API __declspec(dllimport)
#else
#define PSCR_D7_API
#endif

#else
#define PSCR_D7_API
#endif

typedef struct PscrD7 PscrD7;

typedef struct PscrD7OpenOptions {
  int fail_if_exists;
  int create_if_missing;
  int read_only;
  int read_threads;
  const char* backend;
  const char* backend_options;
} PscrD7OpenOptions;

typedef struct PscrD7DatasetInfo {
  const char* name;
  int type;
  int rank;
  int64_t size[3];
} PscrD7DatasetInfo;

typedef struct PscrD7StringList {
  struct PscrD7StringList* next;
  int value_length;
  const char* value;
} PscrD7StringList;

PSCR_D7_API
int pscr_d7_open(const char* path, const PscrD7OpenOptions* oo, PscrD7** file);
PSCR_D7_API
int pscr_d7_close(PscrD7* file);
PSCR_D7_API
int pscr_d7_read(PscrD7* file, const char* dataset_name, int64_t offset,
                 int items, void* data);
PSCR_D7_API
int pscr_d7_append(PscrD7* file, const char* dataset_name, int items,
                   const void* data);

PSCR_D7_API
int pscr_d7_create_dataset(PscrD7* file, const PscrD7DatasetInfo* info);

PSCR_D7_API
int pscr_d7_dataset_names(PscrD7* file, PscrD7StringList** dataset_names);
PSCR_D7_API
int pscr_d7_dataset_info(PscrD7* file, const char* dataset_name,
                         PscrD7DatasetInfo** info);

PSCR_D7_API
int pscr_d7_attribute_names(PscrD7* file, PscrD7StringList** attribute_names);
PSCR_D7_API
int pscr_d7_attribute(PscrD7* file, const char* attribute_name,
                      int* attribute_size, const char** attribute);
PSCR_D7_API
int pscr_d7_set_attribute(PscrD7* file, const char* attribute_name,
                          int attribute_size, const char* attribute);

PSCR_D7_API
void pscr_d7_free(PscrD7* ptr);

PSCR_D7_API
const char* pscr_d7_last_error(PscrD7* file);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PHOTONSCORE_FILE_C_API_H */
