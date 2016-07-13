# Copyright (c) BBP/EPFL 2016 Daniel.Nachbaur@epfl.ch

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(seqSplotch VERSION 1.0.0)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake
                              ${CMAKE_SOURCE_DIR}/CMake/common)
include(GitExternal)

set(seqSplotch_VERSION_ABI 1)

set(SEQSPLOTCH_DESCRIPTION "Example project for the Blue Brain Project")
set(SEQSPLOTCH_MAINTAINER "Blue Brain Project <bbp-open-source@googlegroups.com>")
set(SEQSPLOTCH_LICENSE LGPL BSD)

set(COMMON_PROJECT_DOMAIN ch.epfl.bluebrain)
include(Common)

if(APPLE) # WAR otherwise MacPorts X11 (/opt/local) is preferred
  list(REMOVE_ITEM CMAKE_SYSTEM_LIBRARY_PATH /opt/local/lib /usr/local/lib)
  list(REMOVE_ITEM CMAKE_SYSTEM_PREFIX_PATH /opt/local /usr/local)
  link_directories(/opt/X11/lib /usr/X11R6/lib)
  include_directories(SYSTEM /opt/X11/include /usr/X11R6/include)
endif()

common_find_package(Equalizer REQUIRED)
if(NOT TARGET splotch) # TODO fix find_package(splotch) for superprojects
  common_find_package(splotch REQUIRED)
endif()
common_find_package(OSPRay SYSTEM)
common_find_package(Qt5Widgets)
common_find_package(ZeroBuf REQUIRED)
common_find_package(ZeroEQ)
common_find_package_post()

add_subdirectory(apps)

include(DoxygenRule)
