#pragma once

#ifndef MAIN_H
#define MAIN_H

#include "types.h"

#define SPLIT_APP 1
#define SPLIT_PATCH 2

#define TROPHY_MAGIC 0xDCA24D00
#define NPBIND_MAGIC 0xD294A018

#define bswap16 __builtin_bswap16
#define bswap32 __builtin_bswap32
#define bswap64 __builtin_bswap64

typedef struct {
  int split;
  int notify;
  int shutdown;
} configuration;

struct npbind_header {
  uint32_t magic;
  uint32_t version;
  uint64_t file_size;
  uint64_t entry_size;
  uint64_t num_entries;
  unsigned char padding[96];
};

struct npbind_entry {
  uint16_t type;
  uint16_t size;
  unsigned char data[];
};

struct npbind_body {
  struct npbind_entry npcommid_name;
  struct npbind_entry tophy_number;
  struct npbind_entry unk1;
  struct npbind_entry unk2;
  unsigned char padding[152];
};

struct npbind_file {
  struct npbind_header header;
  struct npbind_body body[];
};

#endif
