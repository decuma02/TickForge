# ==============================================================================
# Benchmarks CMakeLists.txt (Google Benchmark)
# ==============================================================================
add_executable(tickforge_benchmarks
    bench_engine.cpp
    bench_order_book.cpp
)

target_link_libraries(tickforge_benchmarks
    PRIVATE
        tickforge_core
        benchmark::benchmark
)

# Benchmarks should not run with sanitizers overhead in Release to maintain timing accuracy.
# In Debug mode, if tickforge_core has sanitizers enabled, the benchmark binary must link them.
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR ENABLE_SANITIZERS)
    message(WARNING "Building benchmarks in Debug mode will produce skewed timing results. Use -DCMAKE_BUILD_TYPE=Release.")
    configure_target_sanitizers(tickforge_benchmarks)
endif()
