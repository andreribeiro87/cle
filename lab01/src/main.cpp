
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <algorithm>

#include <map>

#include "lz4.h"

#define BLOCK_MiB (512 * 1024 * 1024)

struct ValuesFor
{
    double count = 0.0;
    double max = -std::numeric_limits<double>::infinity();
    double min = std::numeric_limits<double>::infinity();
    double mean = 0.0;
    double M2 = 0.0;
    double variance = 0.0;
    double stddev = 0.0;
};

int main(int argc, char *argv[])
{
    // Use default file ...
    const char *file = "measurements-1.cle";
    if (argc > 1)
    {
        // ... or the first argument.
        file = argv[1];
    }
    std::ifstream fh(file);
    if (not fh.is_open())
    {
        std::cerr << "Unable to open '" << file << "'" << std::endl;
        return 1;
    }
    // Get the number of blocks
    int nblocks;
    fh.read((char *)&nblocks, sizeof(int));

    std::map<std::string, ValuesFor> data;
    // process block by block
    for (int i = 0; i < nblocks; ++i)
    {

        int compressed_size;
        int block_size;
        fh.read((char *)&compressed_size, sizeof(int));
        fh.read((char *)&block_size, sizeof(int));

        // -- Decompress the block data using lz4
        // TODO: Implement
        char *compressed_data = new char[compressed_size];
        fh.read(compressed_data, compressed_size);
        char *decompressed_data = new char[block_size];
        int decompress_size = LZ4_decompress_safe(compressed_data, decompressed_data, compressed_size, block_size);
        if (decompress_size < 0)
        {
            std::cerr << "Unable to decompress data..." << std::endl;
            return 1;
        }
        std::cout << "Block " << i + 1 << ": compressed size = " << compressed_size
                  << " bytes, decompressed size = " << decompress_size << " bytes" << std::endl;

        // -- Process the data
        // TODO: Implement
        /* a dict string, vec, the data has 1 thousand millions */
        std::istringstream iss(std::string(decompressed_data, block_size));

        std::string line;
        while (std::getline(iss, line))
        {
            std::istringstream line_stream(line);
            std::string key;
            // the line is separated by ';', the first part is the key, the second part is the value
            // make a substring of the line until the first ';'
            auto pos = line.find(';');
            double value;
            if (pos == std::string::npos)
            {
                std::cerr << "Invalid line: " << line << std::endl;
                continue;
            }
            key = line.substr(0, pos);
            value = std::stod(line.substr(pos + 1));

            auto &values = data[key];

            values.count += 1.0;
            double delta = value - values.mean;
            values.mean += delta / values.count;
            double delta2 = value - values.mean;
            values.M2 += delta * delta2;
            values.variance = values.M2 / values.count;
            values.stddev = std::sqrt(values.variance);
            values.max = std::max(values.max, value);
            values.min = std::min(values.min, value);
            // std::cout << "Key: " << key << ", Value: " << value << std::endl;
        }
    }

    std::vector<std::pair<std::string, ValuesFor>> sorted_data(data.begin(), data.end());
    std::sort(sorted_data.begin(), sorted_data.end(), [](const auto &a, const auto &b)
              { return a.second.stddev < b.second.stddev; });

    std::ofstream output_file("results.json");
    output_file << "{\n  \"cities\": [\n";

    bool first = true;
    for (auto it = sorted_data.begin(); it != sorted_data.end(); ++it)
    {
        const auto &entry = *it;
        const auto &vals = entry.second;
        double avg = vals.mean;

        if (!first)
            output_file << ",\n";
        first = false;

        output_file << "    {\n";
        output_file << "      \"name\": \"" << entry.first << "\",\n";
        output_file << "      \"avg\": " << avg << ",\n";
        output_file << "      \"std\": " << vals.stddev << ",\n";
        output_file << "      \"min\": " << vals.min << ",\n";
        output_file << "      \"max\": " << vals.max << "\n";
        output_file << "    }";
    }

    output_file << "\n  ]\n}\n";
    output_file.close();

    fh.close();
    return 0;
}
