include(GoogleTest)

add_executable(flint_tests
  bench_test.cpp
  event_signal_test.cpp
  file_watcher_test.cpp
  log_config_test.cpp
  log_filter_test.cpp
  log_formatter_test.cpp
  log_logger_test.cpp
  log_sinks_test.cpp
  meta_enum_test.cpp
  model_class_test.cpp
  model_test.cpp
  platform_test.cpp
  xml_test.cpp
)

target_link_libraries(flint_tests PRIVATE flint::flint GTest::gtest_main GTest::gmock)
flint_target_options(flint_tests)

gtest_discover_tests(flint_tests DISCOVERY_MODE PRE_TEST)

# Every public header must compile on its own (coding-style.md section 5). One generated source per header.
file(GLOB_RECURSE flint_public_headers RELATIVE "${PROJECT_SOURCE_DIR}/include" CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/*.hpp")
# Implementation headers are reached only through their public header.
list(FILTER flint_public_headers EXCLUDE REGEX "/detail/.*_impl\\.hpp$")
set(flint_header_sources "")
foreach(header IN LISTS flint_public_headers)
  string(MAKE_C_IDENTIFIER "${header}" header_id)
  set(source "${CMAKE_CURRENT_BINARY_DIR}/header_check/${header_id}.cpp")
  file(CONFIGURE OUTPUT "${source}" CONTENT "#include \"${header}\"\n")
  list(APPEND flint_header_sources "${source}")
endforeach()
add_library(flint_header_check OBJECT ${flint_header_sources})
target_link_libraries(flint_header_check PRIVATE flint::flint)
flint_target_options(flint_header_check)
