# PicoCraft - ESP32-S3 port (ST7789V, SPI master, PSRAM enabled)
# The following boilerplate lines must be in this exact order.
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Trim the build: include only the minimal set of components plus main and
# everything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

project(picocraft_s3)
