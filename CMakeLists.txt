cmake_minimum_required(VERSION 3.10)
project(CPU_Simulator LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CPU_SOURCES
	src/CPU/CPU.cpp
	src/Decoder/Decoder.cpp
	src/DCache/DCache.cpp
	src/ROB/ROB.cpp
	src/RS/RS.cpp
 	src/ALU/ALU.cpp
 	src/MUL/MUL.cpp
 	src/AGU/AGU.cpp
	src/BRU/BRU.cpp
	src/StaticArbiter/StaticArbiter.cpp
	src/CDB/CDB.cpp
	src/DynamicArbiter/DynamicArbiter.cpp
	src/ICache/ICache.cpp
	src/FetchUnit/FetchUnit.cpp
	src/LQ/LQ.cpp
	src/SQ/SQ.cpp
	src/RAT/RAT.cpp
	src/InstructBuffer/InstructBuffer.cpp
	src/BPU/BPU.cpp
	src/PRF/PRF.cpp
)

add_executable(code
	src/main/main.cpp
	${CPU_SOURCES}
	src/include/DIV.hpp
	src/DIV/DIV.cpp
	src/include/AXIBus.hpp
	src/AXI/AXIBus.cpp
	src/include/AXIData.hpp
	src/AXI/AXIData.cpp
	src/include/AXIFetch.hpp
	src/AXI/AXIFetch.cpp
	src/include/Slave.hpp
	src/AXI/Slave.cpp
)

target_include_directories(code PRIVATE src/include)
target_include_directories(code PRIVATE include)

foreach(setting CPU_FETCH_WIDTH CPU_DECODE_WIDTH CPU_FQ_ENTRIES CPU_IQ_ENTRIES CPU_COMMIT_WIDTH CPU_INT_RS_ENTRIES CPU_BRANCH_RS_ENTRIES CPU_FETCH_REQUEST_ENTRIES CPU_AXI_FETCH_ENTRIES CPU_BTB_ENTRIES CPU_RAS_ENTRIES)
    set(${setting} "" CACHE STRING "Optional CPU compile-time ${setting}")
    if(NOT "${${setting}}" STREQUAL "")
        if(NOT "${${setting}}" MATCHES "^[1-9][0-9]*$")
            message(FATAL_ERROR "${setting} must be a positive integer")
        endif()
        target_compile_definitions(code PRIVATE ${setting}=${${setting}})
    endif()
endforeach()

set(CPU_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}" CACHE PATH
    "Directory for the simulator executable")
set_target_properties(code PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CPU_RUNTIME_OUTPUT_DIRECTORY}"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CPU_RUNTIME_OUTPUT_DIRECTORY}"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CPU_RUNTIME_OUTPUT_DIRECTORY}"
)
