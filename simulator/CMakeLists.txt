cmake_minimum_required(VERSION 3.24)

cmake_policy(SET CMP0091 NEW)

project(
  SmallDesktopDisplaySimulator
  VERSION 1.5.3
  DESCRIPTION "Windows-native simulator for the SmallDesktopDisplay firmware UI"
  LANGUAGES C CXX
)

if(NOT WIN32)
  message(FATAL_ERROR "SmallDesktopDisplaySimulator currently supports Windows only")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

include(FetchContent)

# SDL 3.4.14 is a stable upstream release. Keep the URL and digest pinned so a
# clean build never depends on a moving branch or an unauthenticated archive.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL_DOCS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  SDL3
  URL "https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.zip"
  URL_HASH "SHA256=b07ef7b5431cea0cb2ea9613e2d8410f9ef37f5627f82f1733086cb00aa2b4b4"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL3)

get_filename_component(SDD_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

add_library(
  sdd_simulator_core STATIC
  src/framebuffer.cpp
  src/firmware_assets.cpp
  src/gdiplus_jpeg_decoder.cpp
  src/display_renderer.cpp
  src/simulator_state.cpp
)

target_include_directories(
  sdd_simulator_core
  PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include"
  PRIVATE
    "${SDD_REPOSITORY_ROOT}/src"
    "${CMAKE_CURRENT_LIST_DIR}/shims"
)

target_link_libraries(sdd_simulator_core PRIVATE gdiplus)

add_executable(
  SmallDesktopDisplaySimulator WIN32
  src/simulator_app.cpp
  src/main.cpp
)

target_link_libraries(
  SmallDesktopDisplaySimulator
  PRIVATE
    sdd_simulator_core
    SDL3::SDL3
)

function(sdd_enable_strict_msvc_warnings target_name)
  if(NOT MSVC)
    message(FATAL_ERROR "The Windows simulator currently requires the MSVC toolchain")
  endif()

  target_compile_options(
    "${target_name}"
    PRIVATE
      /W4
      /WX
      /permissive-
      /Zc:__cplusplus
      /utf-8
  )
  target_compile_definitions(
    "${target_name}"
    PRIVATE
      NOMINMAX
      WIN32_LEAN_AND_MEAN
  )
endfunction()

sdd_enable_strict_msvc_warnings(sdd_simulator_core)
sdd_enable_strict_msvc_warnings(SmallDesktopDisplaySimulator)

# SDL is intentionally shipped as an adjacent DLL. The application and SDL are
# both built with the static MSVC runtime (/MT), so users do not need to install
# the Visual C++ Redistributable.
add_custom_command(
  TARGET SmallDesktopDisplaySimulator POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "$<TARGET_FILE:SDL3::SDL3-shared>"
          "$<TARGET_FILE_DIR:SmallDesktopDisplaySimulator>"
  VERBATIM
)

include(CTest)
if(BUILD_TESTING)
  add_executable(
    simulator_tests
    tests/simulator_tests.cpp
  )
  target_link_libraries(simulator_tests PRIVATE sdd_simulator_core)
  sdd_enable_strict_msvc_warnings(simulator_tests)

  add_test(NAME simulator_tests COMMAND simulator_tests)
  set_tests_properties(
    simulator_tests
    PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
  )
endif()

install(TARGETS SmallDesktopDisplaySimulator RUNTIME DESTINATION .)
install(FILES "$<TARGET_FILE:SDL3::SDL3-shared>" DESTINATION .)
install(FILES
  "${CMAKE_CURRENT_LIST_DIR}/README.md"
  "${CMAKE_CURRENT_LIST_DIR}/THIRD_PARTY_NOTICES.txt"
  "${SDD_REPOSITORY_ROOT}/LICENSE"
  DESTINATION .
)

set(CPACK_GENERATOR ZIP)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGE_NAME "SmallDesktopDisplaySimulator")
set(CPACK_PACKAGE_VENDOR "SmallDesktopDisplay")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "SmallDesktopDisplaySimulator-${PROJECT_VERSION}-windows-x64")
set(CPACK_PACKAGE_CHECKSUM SHA256)
include(CPack)
