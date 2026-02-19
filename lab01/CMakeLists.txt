cmake_minimum_required(VERSION 3.22.1)
project(cle-weather-stations C CXX)

add_executable(cle-ws
    src/main.cpp
    src/lz4.c
    # Add your files here if needed.
)
target_compile_features(cle-ws PRIVATE cxx_std_20)
target_compile_options(cle-ws PRIVATE -Wall -Wextra) # To see -O3 -march=native

add_executable(cle-samples src/lz4.c src/create-samples.cpp)
target_compile_features(cle-samples PRIVATE cxx_std_20)
target_compile_options(cle-samples PRIVATE -Wall -Wextra)
