cmake_minimum_required(VERSION 3.16.3)
project(rangefilters-bench)

option(ALL_RENCODER "compile all REncoder implementation (SE/SS)" ON)

# fetch latest argparse
include(FetchContent)
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)

set(CMAKE_CXX_STANDARD 17)

list(APPEND Targets "grafite" "surf" "rosetta" "proteus" "snarf" "rencoder" "bucketing")
list(APPEND x86Targets "surf" "rosetta" "proteus" "rencoder")

function(compile_bench ds)
    if (NOT ds IN_LIST Targets)
        message(WARNING "${ds} is not defined")
        return()
    endif()
    if (ds IN_LIST x86Targets AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
        message(WARNING "cannot compile ${ds} on this architecture")
        return()
    endif()
    add_executable(bench_${ds} filters_benchmark/bench_${ds}.cpp)
    target_link_libraries(bench_${ds} argparse)

    if (ds MATCHES "grafite")
        target_link_libraries(bench_grafite grafitelib)
    elseif (ds MATCHES "bucketing")
        target_link_libraries(bench_bucketing grafitelib)
    elseif (ds MATCHES "rosetta")
        add_library(rosetta STATIC ./include/Rosetta/dst.cpp ./include/Rosetta/MurmurHash3.cpp)
        target_link_libraries(bench_rosetta rosetta)
    elseif(ds MATCHES "proteus")
        add_library(proteus STATIC ./include/Proteus/include/config.cpp ./include/Proteus/include/clhash.cpp ./include/Proteus/include/prefixbf.cpp ./include/Proteus/include/MurmurHash3.cpp ./include/Proteus/include/util.cpp ./include/Proteus/include/modeling.cpp)
        target_link_libraries(bench_proteus proteus)
    elseif(ds MATCHES "snarf" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
        link_directories("/opt/homebrew/lib")
        include_directories("/opt/homebrew/include")
    endif()
endfunction(compile_bench)

foreach(ds ${Targets})
    compile_bench(${ds})
endforeach()

if (ALL_RENCODER AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    add_executable(bench_rencoder_se filters_benchmark/bench_rencoder.cpp)
    target_compile_definitions(bench_rencoder_se PRIVATE SET_VERSION="REncoderSE")
    target_link_libraries(bench_rencoder_se argparse)
    add_executable(bench_rencoder_ss filters_benchmark/bench_rencoder.cpp)
    target_compile_definitions(bench_rencoder_ss PRIVATE SET_VERSION="REncoderSS")
    target_link_libraries(bench_rencoder_ss argparse)
endif()

add_executable(workload_gen workload_gen.cpp)
target_link_libraries(workload_gen argparse)