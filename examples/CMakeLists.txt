#-----------------------------------------------------------------------------
# Q2NS - Quantum Network Simulator
# Copyright (c) 2026 quantuminternet.it
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#---------------------------------------------------------------------------*/

#-----------------------------------------------------------------------------
# Q2NS: Quantum Network Simulator
# Examples CMake configuration
#-----------------------------------------------------------------------------

# Common libs most examples need
set(Q2NS_EXAMPLE_LIBS ${libq2ns} ${libinternet} ${libnetwork} ${libcore})

# Find all example mains (only those with -example.cc at the end to avoid grabbing helper .cc files)
file(GLOB examples CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*-example.cc")

foreach(src IN LISTS examples)
  get_filename_component(name "${src}" NAME_WE)
  message(STATUS "Registering example: ${name}")

  build_lib_example(
    NAME         "${name}"
    SOURCE_FILES "${src}"
    LIBRARIES_TO_LINK
      ${Q2NS_EXAMPLE_LIBS}
  )
endforeach()