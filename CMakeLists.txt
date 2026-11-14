cmake_minimum_required(VERSION 3.25)
project(wwe13 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

find_package(Git QUIET)
set(WWE13_BUILD_COMMIT unknown)
set(WWE13_BUILD_DIRTY unknown)
set(WWE13_SDK_COMMIT unknown)
set(WWE13_SDK_DIRTY unknown)
if(GIT_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE WWE13_BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE WWE13_GIT_RESULT ERROR_QUIET)
  if(NOT WWE13_GIT_RESULT EQUAL 0)
    set(WWE13_BUILD_COMMIT unknown)
  else()
    execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE WWE13_GIT_STATUS OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE WWE13_GIT_STATUS_RESULT ERROR_QUIET)
    if(WWE13_GIT_STATUS_RESULT EQUAL 0)
      if(WWE13_GIT_STATUS STREQUAL "")
        set(WWE13_BUILD_DIRTY no)
      else()
        set(WWE13_BUILD_DIRTY yes)
      endif()
    endif()
  endif()

  if(NOT DEFINED WWE13_SDK_ROOT OR WWE13_SDK_ROOT STREQUAL "")
    set(WWE13_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../tools/rexglue")
  endif()
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${WWE13_SDK_ROOT}"
    OUTPUT_VARIABLE WWE13_SDK_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE WWE13_SDK_GIT_RESULT ERROR_QUIET)
  if(WWE13_SDK_GIT_RESULT EQUAL 0)
    execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain
      WORKING_DIRECTORY "${WWE13_SDK_ROOT}"
      OUTPUT_VARIABLE WWE13_SDK_GIT_STATUS OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE WWE13_SDK_GIT_STATUS_RESULT ERROR_QUIET)
    if(WWE13_SDK_GIT_STATUS_RESULT EQUAL 0)
      if(WWE13_SDK_GIT_STATUS STREQUAL "")
        set(WWE13_SDK_DIRTY no)
      else()
        set(WWE13_SDK_DIRTY yes)
      endif()
    endif()
  endif()
endif()
include(cmake/wwe13_version.cmake)
string(TIMESTAMP WWE13_BUILD_DATE "%Y-%m-%dT%H:%M:%SZ" UTC)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/wwe13_build_info.h.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/wwe13_build_info.h" @ONLY)

set(WWE13_SOURCES
  src/main.cpp
  src/game_version_check.cpp
  src/release_diagnostics.cpp
  src/single_instance.cpp
  src/dlist_tripwire.cpp
  src/perf_overlay.cpp
  src/thread_profiler.cpp
  src/bink_oracle.cpp
  src/resolve_coherency.cpp
  src/present_interval.cpp
  src/cloth_handshake.cpp
  src/res_probe.cpp
  src/scene_resolution.cpp
  src/text_entry_keyboard.cpp
  src/scene_aa.cpp
  src/xmp_audio_probe.cpp
  src/shader_cache_seed.cpp)

if(ANDROID)
  list(APPEND WWE13_SOURCES src/virtual_gamepad.cpp)
  add_library(main SHARED ${WWE13_SOURCES})
  target_compile_definitions(main PRIVATE REX_ANDROID_APP_ID="wwe13")
  set(WWE13_TARGET main)
else()
  add_executable(wwe13 ${WWE13_SOURCES})
  set(WWE13_TARGET wwe13)
endif()

target_include_directories(${WWE13_TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
rexglue_setup_target(${WWE13_TARGET})

if(ANDROID)
  find_package(SDL3 CONFIG REQUIRED)
  if(NOT TARGET SDL3::SDL3-shared)
    message(FATAL_ERROR "Installed SDK does not export SDL3::SDL3-shared")
  endif()

  set(WWE13_LAUNCHER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/launcher2")
  set(WWE13_IMGUI_SOURCE
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/rexglue/thirdparty/imgui")

  add_library(wwe13_launcher_imgui STATIC
    "${WWE13_IMGUI_SOURCE}/imgui.cpp"
    "${WWE13_IMGUI_SOURCE}/imgui_draw.cpp"
    "${WWE13_IMGUI_SOURCE}/imgui_tables.cpp"
    "${WWE13_IMGUI_SOURCE}/imgui_widgets.cpp"
    "${WWE13_IMGUI_SOURCE}/backends/imgui_impl_sdl3.cpp"
    "${WWE13_IMGUI_SOURCE}/backends/imgui_impl_sdlrenderer3.cpp"
  )
  target_include_directories(wwe13_launcher_imgui PUBLIC
    "${WWE13_IMGUI_SOURCE}"
    "${WWE13_IMGUI_SOURCE}/backends"
  )
  target_link_libraries(wwe13_launcher_imgui PUBLIC SDL3::SDL3-shared)

  add_subdirectory("${WWE13_LAUNCHER_DIR}/core"
                   "${CMAKE_CURRENT_BINARY_DIR}/launcher-core")

  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  find_program(WWE13_HOST_CXX_COMPILER NAMES clang++ g++ REQUIRED
               NO_CMAKE_FIND_ROOT_PATH)

  set(WWE13_FONT_COMPRESSOR
      "${CMAKE_CURRENT_BINARY_DIR}/wwe13-font-compressor-host")
  add_custom_command(
    OUTPUT "${WWE13_FONT_COMPRESSOR}"
    COMMAND "${WWE13_HOST_CXX_COMPILER}" -std=c++11
      "${WWE13_IMGUI_SOURCE}/misc/fonts/binary_to_compressed_c.cpp"
      -o "${WWE13_FONT_COMPRESSOR}"
    DEPENDS "${WWE13_IMGUI_SOURCE}/misc/fonts/binary_to_compressed_c.cpp"
    VERBATIM
  )
  add_custom_target(wwe13_launcher_font_compressor
                    DEPENDS "${WWE13_FONT_COMPRESSOR}")

  set(WWE13_FONT_FILES
    "${WWE13_LAUNCHER_DIR}/ui/fonts/Oswald-Medium.ttf"
    "${WWE13_LAUNCHER_DIR}/ui/fonts/Oswald-SemiBold.ttf"
    "${WWE13_LAUNCHER_DIR}/ui/fonts/Oswald-Bold.ttf"
    "${WWE13_LAUNCHER_DIR}/ui/fonts/Barlow-Regular.ttf"
    "${WWE13_LAUNCHER_DIR}/ui/fonts/Barlow-Medium.ttf"
    "${WWE13_LAUNCHER_DIR}/ui/fonts/Barlow-SemiBold.ttf"
    "${WWE13_LAUNCHER_DIR}/ui/fonts/NotoSans-Variable.ttf"
    "${WWE13_LAUNCHER_DIR}/ui/fonts/NotoSansSymbols2-Regular.ttf"
  )
  set(WWE13_EMBEDDED_FONTS
      "${CMAKE_CURRENT_BINARY_DIR}/launcher_embedded_fonts.h")
  add_custom_command(
    OUTPUT "${WWE13_EMBEDDED_FONTS}"
    COMMAND "${Python3_EXECUTABLE}"
      "${WWE13_LAUNCHER_DIR}/ui/generate_fonts.py"
      "${WWE13_FONT_COMPRESSOR}"
      "${WWE13_LAUNCHER_DIR}/ui/fonts"
      "${WWE13_EMBEDDED_FONTS}"
    DEPENDS wwe13_launcher_font_compressor
      "${WWE13_LAUNCHER_DIR}/ui/generate_fonts.py"
      ${WWE13_FONT_FILES}
    VERBATIM
  )
  add_custom_target(wwe13_launcher_fonts DEPENDS "${WWE13_EMBEDDED_FONTS}")

  include("${WWE13_LAUNCHER_DIR}/../cmake/wwe13_version.cmake")
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/launcher-version")
  configure_file(
    "${WWE13_LAUNCHER_DIR}/launcher_version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/launcher-version/launcher_version.h"
    @ONLY
  )

  target_include_directories(wwe13_launcher_core PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/launcher-version"
  )

  target_sources(main PRIVATE
    "${WWE13_LAUNCHER_DIR}/ui/main.cpp"
    "${WWE13_LAUNCHER_DIR}/ui/native_keys.cpp"
    "${WWE13_EMBEDDED_FONTS}"
  )
  target_include_directories(main PRIVATE
    "${WWE13_LAUNCHER_DIR}/core"
    "${WWE13_LAUNCHER_DIR}/ui"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/launcher-version"
  )
  target_link_libraries(main PRIVATE
    wwe13_launcher_core
    wwe13_launcher_imgui
  )
  add_dependencies(main wwe13_launcher_fonts)
endif()

if(WIN32)
  target_link_libraries(wwe13 PRIVATE advapi32)
  # Link map next to the exe: symbol -> object resolution for Windows-only link checks.
  target_link_options(wwe13 PRIVATE "LINKER:/map:$<TARGET_FILE_DIR:wwe13>/wwe13.map")
endif()
