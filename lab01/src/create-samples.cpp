
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <iomanip>
#include <random>
#include <sstream>
#include <cstring>

#include "cities.cpp"

#include "lz4.h"

/*
 *
 * == DATA FORMAT ==
 * Input value ranges are as follows:
 * - Station name: non null UTF-8 string of min length 1 character and max length
 *   100 bytes, containing neither ; nor \n characters. (i.e. this could be 100
 *   one-byte characters, or 50 two-byte characters, etc.)
 * - Temperature value: non null double between -99.9 (inclusive) and 99.9 (inclusive),
 *   always with one fractional digit.
 *
 * - There is a maximum of 10,000 unique station names
 * - Line endings in the file are \n characters on all platforms
 *
 *
 * == DISK FORMAT ==
 *
 *   nblock   compr.    orig            data
 * |--------|--------|--------|------------------------| ...
 *          | <----------- block n ------------------> |
 *
 * A sample file starts with an integer that contains the number of blocks.
 * Then, each block starts with the size of the compressed data, followed by
 * the size of the original data (i.e., not compressed) and finalized by the
 * compressed data.
 */

/*
 *
 */

#define BLOCK_SZ (512 * 1024 * 1024)

static std::random_device rnd;
static std::mt19937 gen(rnd());

double rand_normal(double mean, double stddev)
{
    std::normal_distribution<double> dist(mean, stddev);
    return dist(gen);
}

int rand_uniform(int min, int max)
{
    std::uniform_int_distribution dist(min, max);
    return dist(gen);
}

double rand_uniform(double min, double max)
{
    std::uniform_real_distribution dist(min, max);
    return dist(gen);
}

int main(int argc, char *argv[])
{
    // 1. Number of blocks to generate as argument
    if (argc < 2)
    {
        std::cerr << "Missing argument: create-sample N_BLOCKS" << std::endl;
        std::cerr << "Each block contains approximately " << BLOCK_SZ / 1024 / 1024 << "MiB of data" << std::endl;
        return 1;
    }

    int nblocks = atoi(argv[1]);
    if (nblocks <= 0)
        nblocks = 1;
    else if (nblocks > 100)
        nblocks = 100;

    int ncities = sizeof(city_data) / sizeof(city_data[0]);
    // Go through all cities and set a random standard deviation
    for (int i = 0; i < ncities; ++i)
        city_data[i].std = rand_uniform(1.0, 4.0);

    unsigned int nsamples = 0;
    char *data = new char[BLOCK_SZ];
    std::memset(data, 0, BLOCK_SZ);

    // 2. Create the file
    std::stringstream ss;
    ss << "measurements-" << nblocks << ".cle";
    std::string fname = ss.str();

    int fd = open(fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    // We need to know how many block to load
    write(fd, (void *)&nblocks, sizeof(nblocks));

    std::cout << "Generating " << nblocks << " block(s) ..." << std::endl;
    for (int i = 0; i < nblocks; ++i)
    {
        int blk_size = 0;

        while (blk_size <= BLOCK_SZ)
        {

            int select = rand_uniform(0, ncities - 1);
            char text[512];

            int text_size = std::snprintf(text, 512, "%s;%.1f\n",
                                          city_data[select].city,
                                          rand_normal(city_data[select].mean, city_data[select].std));

            if ((blk_size + text_size) <= BLOCK_SZ)
            {

                std::strncpy(data + blk_size, text, text_size);
                blk_size += text_size;
                nsamples += 1;
            }
            else
            {
                std::cout << " writting block " << i + 1 << std::endl;

                // Time to a) compress the data and b) write it to the file
                auto alloc_sz = LZ4_compressBound(blk_size);
                char *dst = new char[alloc_sz];

                int new_sz = LZ4_compress_default(data, dst, blk_size, alloc_sz);
                if (new_sz == 0)
                {
                    std::cerr << "Unable to compress data..." << std::endl;
                    return 1;
                }

                write(fd, (void *)&new_sz, sizeof(new_sz));
                write(fd, (void *)&blk_size, sizeof(blk_size));

                int ret = write(fd, dst, new_sz);
                if (ret != new_sz)
                {
                    std::cerr << "Unable to write to file." << std::endl;
                    return 1;
                }

                delete[] dst;
                blk_size = BLOCK_SZ + text_size;
                continue;
            }
        } // end while(blk_size <= BLOCK_SZ)
    } // end for

    close(fd);
    delete[] data;

    // Done
    std::cout << "Done: " << nsamples << " city samples" << std::endl;

    return 0;
}
