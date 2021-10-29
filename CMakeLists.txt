# CMake file for 2040-eight, a puzzle game for the PicoSystem.
#
# Copyright (c) 2021 Pete Favelle <picosystem@ahnlak.com>
# This file is distributed under the MIT License; see LICENSE for details.

# Imagine we could handle any modern version, but CMake complains if we don't say
cmake_minimum_required(VERSION 3.12)

# Make sure we're set to a suitable board type
set(PICO_BOARD pimoroni_picosystem)

# Pull in PICO SDK (must be before project)
include(${PICOSYSTEM_DIR}/pico_sdk_import.cmake)

# Define the project, including which standards to apply
project(2040-eight      C CXX ASM)
set(CMAKE_C_STANDARD    11)
set(CMAKE_CXX_STANDARD  17)

# Initialise the Pico side of things
pico_sdk_init()

# Latch onto the PicoSystem SDK stuff
find_package(PICOSYSTEM REQUIRED)

# Define the source files we build from
picosystem_executable(2040-eight
  2040-eight.cpp 
)

# Set some Pico version info
pico_set_program_name(2040-eight "2040-eight")
pico_set_program_version(2040-eight "v0.3")

#pixel_double(2040-eight)
no_spritesheet(2040-eight)
disable_startup_logo(2040-eight)
