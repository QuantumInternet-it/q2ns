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
# CMake configuration
#-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_filename_component(Q2NS_THIS_DIR "${CMAKE_CURRENT_LIST_DIR}" REALPATH)

# --- Sources -----------------------------------------------------------------
file(GLOB q2ns_sources RELATIVE ${CMAKE_CURRENT_LIST_DIR} CONFIGURE_DEPENDS
  model/*.cc
  model/*.cpp
  helper/*.cc
)

if(NOT q2ns_sources)
  message(FATAL_ERROR "q2ns: no source files found in model/ or helper/")
endif()

# --- Public headers ----------------------------------------------------------
file(GLOB q2ns_public_headers RELATIVE ${CMAKE_CURRENT_LIST_DIR} CONFIGURE_DEPENDS
  model/q2ns-analysis.h
  model/q2ns-netcontroller.h
  model/q2ns-qchannel.h
  model/q2ns-qgate.h
  model/q2ns-qmap.h
  model/q2ns-qnet-device.h
  model/q2ns-qnode.h
  model/q2ns-qstate-all.h
  model/q2ns-qstate-dm.h
  model/q2ns-qstate-ket.h
  model/q2ns-qstate-stab.h
  model/q2ns-qstate.h
  model/q2ns-qubit.h
  model/q2ns-types.h

  model/q2ns-qstate-registry.h

  model/q2nsviz-trace.h
  model/q2nsviz-trace-writer.h

  model/q2ns-swap-app.h
  model/q2ns-teleportation-app.h

  helper/*.h
  helper/*.hpp
)

# --- Private headers (IDE visibility only) -----------------------------------
file(GLOB q2ns_private_headers RELATIVE ${CMAKE_CURRENT_LIST_DIR} CONFIGURE_DEPENDS
  model/q2ns-qnetworker.h
  model/q2ns-qprocessor.h
)

# --- Eigen -------------------------------------------------------------------
set(EIGEN_INCLUDE_DIR "${Q2NS_THIS_DIR}/third_party/eigen")
if(NOT EXISTS "${EIGEN_INCLUDE_DIR}/Eigen")
  message(FATAL_ERROR
    "Vendored Eigen not found at ${EIGEN_INCLUDE_DIR}"
  )
endif()

# --- qpp include path (header-only) ------------------------------------------
set(QPP_INCLUDE_DIR "${Q2NS_THIS_DIR}/third_party/qpp/include" CACHE PATH
  "Directory that contains qpp/qpp.hpp"
)

if(NOT EXISTS "${QPP_INCLUDE_DIR}/qpp/qpp.hpp")
  message(FATAL_ERROR
    "qpp headers not found at ${QPP_INCLUDE_DIR}. "
    "Expected vendored copy at third_party/qpp/include"
  )
endif()

# --- qasmtools include path (header-only) ------------------------------------
set(QASMTOOLS_INCLUDE_DIR "${Q2NS_THIS_DIR}/third_party/qasmtools/include" CACHE PATH
  "Directory that contains qasmtools/parser/parser.hpp"
)

if(NOT EXISTS "${QASMTOOLS_INCLUDE_DIR}/qasmtools/parser/parser.hpp")
  message(FATAL_ERROR
    "qasmtools headers not found at ${QASMTOOLS_INCLUDE_DIR}. "
    "Expected vendored copy at third_party/qasmtools/include"
  )
endif()

message(STATUS "qasmtools: using headers at ${QASMTOOLS_INCLUDE_DIR}")

# --- stab (vendored MIT stabilizer backend) ----------------------------------
set(STAB_DIR "${Q2NS_THIS_DIR}/third_party/stab")

set(stab_sources
  "${STAB_DIR}/AffineState.cpp"
  "${STAB_DIR}/random.cpp"
)

foreach(f IN LISTS stab_sources)
  if(NOT EXISTS "${f}")
    message(FATAL_ERROR "Missing stab source: ${f}")
  endif()
endforeach()

list(APPEND q2ns_sources ${stab_sources})

# Maximum number of qubits allowed in qpp::AffineState::to_ket()
set(MAX_QUBITS_STATE_VECTOR
    "16"
    CACHE STRING "Maximum number of qubits allowed in AffineState::to_ket()")
add_compile_definitions(MAX_QUBITS_STATE_VECTOR=${MAX_QUBITS_STATE_VECTOR})

# --- Test sources ------------------------------------------------------------
file(GLOB q2ns_test_sources RELATIVE ${CMAKE_CURRENT_LIST_DIR} CONFIGURE_DEPENDS
  test/*-test-suite.cc
)

# --- Build q2ns --------------------------------------------------------------
build_lib(
  LIBNAME      q2ns
  SOURCE_FILES ${q2ns_sources}
  HEADER_FILES ${q2ns_public_headers} ${q2ns_private_headers}
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libpoint-to-point}
    ${libcsma}
    ${libapplications}
    ${libstats}

  TEST_SOURCES ${q2ns_test_sources}
)

# --- Language, includes, macros ---------------------------------------------
target_compile_features(${libq2ns} PUBLIC cxx_std_23)
target_compile_definitions(${libq2ns} PUBLIC USE_QPP=1)

target_include_directories(${libq2ns} PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/model"
  "${CMAKE_CURRENT_LIST_DIR}/helper"
)

target_include_directories(${libq2ns} PUBLIC
  $<BUILD_INTERFACE:${EIGEN_INCLUDE_DIR}>
  $<BUILD_INTERFACE:${QPP_INCLUDE_DIR}>
  $<BUILD_INTERFACE:${QASMTOOLS_INCLUDE_DIR}>
)
target_include_directories(${libq2ns} PRIVATE "${STAB_DIR}")

# --- Examples ----------------------------------------------------------------
if(NS3_ENABLE_EXAMPLES)
  add_subdirectory(examples)
endif()

# --- Experiments (optional, not part of standard release) --------------------
option(NS3_Q2NS_EXPERIMENTS "Build q2ns research experiments" OFF)
if(NS3_Q2NS_EXPERIMENTS AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/experiments/CMakeLists.txt")
  add_subdirectory(experiments)
endif()