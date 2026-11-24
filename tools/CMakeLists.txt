add_executable(flint-logcat logcat/main.cpp)
target_link_libraries(flint-logcat PRIVATE flint::flint)
flint_target_options(flint-logcat)
