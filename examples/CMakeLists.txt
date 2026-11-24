add_executable(flint_log_demo log_demo.cpp)
target_link_libraries(flint_log_demo PRIVATE flint::flint)
flint_target_options(flint_log_demo)

add_executable(flint_bench_demo bench_demo.cpp)
target_link_libraries(flint_bench_demo PRIVATE flint::flint)
flint_target_options(flint_bench_demo)

# The demo reads its configuration from next to the executable.
configure_file(log_demo.json ${CMAKE_CURRENT_BINARY_DIR}/log_demo.json COPYONLY)
