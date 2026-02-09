
#include <iostream>
#include <fstream>

#include "lz4.h"

#define BLOCK_MiB (512 * 1024 * 1024)

int main(int argc, char* argv[])
{
    // Use default file ...
    const char* file = "measurements-1.cle";
    if (argc > 1){
        // ... or the first argument.
        file = argv[1];
    }
    std::ifstream fh(file);
    if (not fh.is_open()){
        std::cerr << "Unable to open '" << file << "'" << std::endl;
        return 1;
    }
    // Get the number of blocks
    int nblocks;
    fh.read((char*)&nblocks, sizeof(int));

    // process block by block
    for (int i = 0; i < nblocks; ++i){

        int compressed_size;
        int block_size;
        fh.read((char*)&compressed_size, sizeof(int));
        fh.read((char*)&block_size, sizeof(int));

        // -- Decompress the block data using lz4
        // TODO: Implement

        // -- Process the data
        // TODO: Implement
    }

    // Always close the file when done
    fh.close();

    return 0;
}
