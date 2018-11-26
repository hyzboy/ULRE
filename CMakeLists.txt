cmake_minimum_required(VERSION 3.0)

PROJECT(ULRE VERSION 1)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(system_bit)
include(version)
include(compiler)
include(output_path)

check_system_bits()
check_system_version()
set_compiler_param()
set_output_directory()

