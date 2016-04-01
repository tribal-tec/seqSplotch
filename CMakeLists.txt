# Copyright (c) BBP/EPFL 2016 Daniel.Nachbaur@epfl.ch

# //! [Setup]
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(seqSplotch)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake
                              ${CMAKE_SOURCE_DIR}/CMake/common)
include(GitExternal)

set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_PATCH "0")
set(VERSION_ABI 1)

set(SEQSPLOTCH_DESCRIPTION "Example project for the Blue Brain Project")
set(SEQSPLOTCH_MAINTAINER "Blue Brain Project <bbp-open-source@googlegroups.com>")
set(SEQSPLOTCH_LICENSE LGPL BSD)

set(COMMON_PROJECT_DOMAIN ch.epfl.bluebrain)
include(Common)
common_package(Equalizer REQUIRED)
if(NOT TARGET splotch)
  common_package(splotch REQUIRED)
endif()
common_package_post()

add_subdirectory(apps)

include(DoxygenRule)
