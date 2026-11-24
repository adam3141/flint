# Compiler options shared by flint and libraries built on it (agate, opal).
# Warning set: docs/architecture/coding-style.md section 2.
include_guard(GLOBAL)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++20 modules yet; scanning adds GCC-only flags that clangd rejects.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Applies the workspace warning set, treated as errors, and any configured sanitizers to a target.
function(flint_target_options target)
  # No C++20 modules yet; scanning adds GCC-only flags that clangd and clang-tidy reject.
  set_target_properties(${target} PROPERTIES CXX_SCAN_FOR_MODULES OFF)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /WX /utf-8)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wimplicit-fallthrough
      -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wnull-dereference
      -Werror)
  endif()

  # Bounds-check standard containers in debug builds (stands in for cppcoreguidelines-pro-bounds-* checks).
  if(NOT MSVC)
    target_compile_definitions(${target} PRIVATE $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
  endif()

  if(FLINT_SANITIZERS)
    list(JOIN FLINT_SANITIZERS "," sanitizers)
    target_compile_options(${target} PRIVATE -fsanitize=${sanitizers} -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=${sanitizers})
  endif()
endfunction()

# Enables C++26 static reflection. GCC needs -freflection; the flag is PUBLIC because reflection code lives in headers
# (flint/meta). See flint ADR-0003 for how clang-based tooling is kept working.
function(flint_enable_reflection target)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
      message(FATAL_ERROR "flint requires GCC 16 or newer for C++26 static reflection")
    endif()
    target_compile_options(${target} PUBLIC -freflection)
  else()
    message(WARNING "${CMAKE_CXX_COMPILER_ID} may not support C++26 static reflection; flint/meta will not compile")
  endif()
endfunction()
