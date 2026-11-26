cmake_minimum_required(VERSION 3.24)
project(VeyraCacheProbe LANGUAGES CXX)
add_library(cache_probe OBJECT cache_probe.cpp)
target_compile_features(cache_probe PRIVATE cxx_std_20)
