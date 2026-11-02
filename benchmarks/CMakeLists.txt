# ==============================================================================
# Benchmarks CMakeLists.txt (Google Benchmark)
# ==============================================================================
add_executable(tickforge_benchmarks
    bench_engine.cpp
)

target_link_libraries(tickforge_benchmarks
    PRIVATE
        tickforge_core
        benchmark::benchmark
)

# Benchmarks should not run with sanitizers overhead to maintain timing accuracy
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "Building benchmarks in Debug mode will produce skewed timing results. Use -DCMAKE_BUILD_TYPE=Release.")
endif()
