/**
 * cle-ws  –  Weather Station Aggregator
 *   <station_name>;<temperature>\n
 *   temperature  ::=  [-][0-9]{1,2}'.'[0-9]       (e.g. -3.7, 15.4, -99.9)
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#include "lz4.h"
#include "fast_parse.h" // StationMap, process_block
// stats.h is pulled in transitively via fast_parse.h

int main(int argc, char *argv[])
{
  const char *file = (argc > 1) ? argv[1] : "measurements-1.cle";

  std::FILE *fh = std::fopen(file, "rb");
  if (!fh)
  {
    std::fprintf(stderr, "Unable to open '%s'\n", file);
    return 1;
  }

  int32_t nblocks = 0;
  if (std::fread(&nblocks, sizeof(int32_t), 1, fh) != 1)
  {
    std::fprintf(stderr, "Failed to read block count\n");
    std::fclose(fh);
    return 1;
  }

  std::vector<char> compressed_buf;
  std::vector<char> decompressed_buf;

  compressed_buf.reserve(256 * 1024 * 1024);       // 256 MiB
  decompressed_buf.reserve(512 * 1024 * 1024 + 1); // 512 MiB + sentinel

  StationMap data;
  data.reserve(16384); // power-of-2 >= 10 000 avoids rehashing

  for (int32_t i = 0; i < nblocks; ++i)
  {
    int32_t compressed_size = 0;
    int32_t decompressed_size = 0;

    if (std::fread(&compressed_size, sizeof(int32_t), 1, fh) != 1 ||
        std::fread(&decompressed_size, sizeof(int32_t), 1, fh) != 1)
    {
      std::fprintf(stderr, "Block %d: failed to read header\n", i + 1);
      std::fclose(fh);
      return 1;
    }

    if (static_cast<int>(compressed_buf.size()) < compressed_size)
      compressed_buf.resize(static_cast<std::size_t>(compressed_size));

    if (static_cast<int>(decompressed_buf.size()) < decompressed_size)
      decompressed_buf.resize(static_cast<std::size_t>(decompressed_size));

    if (std::fread(compressed_buf.data(), 1,
                   static_cast<std::size_t>(compressed_size), fh) != static_cast<std::size_t>(compressed_size))
    {
      std::fprintf(stderr, "Block %d: failed to read compressed data\n", i + 1);
      std::fclose(fh);
      return 1;
    }

    const int actual_size = LZ4_decompress_safe(
        compressed_buf.data(),
        decompressed_buf.data(),
        compressed_size,
        decompressed_size);

    if (actual_size < 0)
    {
      std::fprintf(stderr, "Block %d: LZ4 decompression failed (error %d)\n",
                   i + 1, actual_size);
      std::fclose(fh);
      return 1;
    }

    process_block(decompressed_buf.data(), actual_size, data);
  }
  std::fclose(fh);

  std::vector<std::pair<const std::string *, const ValuesFor *>> sorted;
  sorted.reserve(data.size());
  for (const auto &kv : data)
    sorted.emplace_back(&kv.first, &kv.second);

  std::sort(sorted.begin(), sorted.end(),
            [](const auto &a, const auto &b)
            {
              return a.second->stddev() < b.second->stddev();
            });

  std::FILE *out = std::fopen("results.json", "w");
  if (!out)
  {
    std::fprintf(stderr, "Unable to open results.json for writing\n");
    return 1;
  }

  std::fputs("{\"cities\":[\n", out);

  for (std::size_t idx = 0; idx < sorted.size(); ++idx)
  {
    const auto &name = *sorted[idx].first;
    const auto &vals = *sorted[idx].second;

    if (idx > 0)
      std::fputs(",\n", out);

    std::fprintf(out,
                 "{\"name\":\"%s\","
                 "\"avgStationMap\":%.10g,"
                 "\"std\":%.10g,"
                 "\"min\":%.1f,"
                 "\"max\":%.1f}",
                 name.c_str(),
                 vals.mean,
                 vals.stddev(),
                 vals.min(),
                 vals.max());
  }

  std::fputs("\n]}\n", out);
  std::fclose(out);

  return 0;
}
