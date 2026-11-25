# Third-party dependencies, pinned to release tags and fetched with FetchContent (flint ADR-0002).
include_guard(GLOBAL)

include(FetchContent)

# Third-party code is compiled without the workspace warning set, and must not inherit -Werror.
set(FETCHCONTENT_QUIET ON)

# XML parsing (flint ADR-0011). Only the DOM parser is used, so XPath is left out.
FetchContent_Declare(pugixml
  GIT_REPOSITORY https://github.com/zeux/pugixml.git
  GIT_TAG v1.16
  GIT_SHALLOW TRUE
  SYSTEM
)
set(PUGIXML_NO_XPATH ON CACHE BOOL "" FORCE)
set(PUGIXML_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(pugixml)

if(FLINT_ENABLE_TRACY)
  FetchContent_Declare(tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.15.0
    GIT_SHALLOW TRUE
    SYSTEM
  )
  set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
  set(TRACY_ON_DEMAND ON CACHE BOOL "Only collect when a profiler is connected" FORCE)
  FetchContent_MakeAvailable(tracy)
endif()

if(FLINT_BUILD_TESTS)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
    GIT_SHALLOW TRUE
    SYSTEM
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()
