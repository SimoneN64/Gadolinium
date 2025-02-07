cmake_minimum_required(VERSION 3.20)
project(kaizen-qt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

if (WIN32)
  add_compile_definitions(NOMINMAX)
endif ()

if(APPLE)
  enable_language(OBJC)
endif()

include_directories(
  .
  ../
  ../utils
  ../backend
  ../backend/core
  ../backend/core/mmio
  ../backend/core/mmio/PIF
  ../backend/core/registers
  ../backend/core/rsp
  ../../external
  ../../external/discord_rpc/include
  ../../external/xbyak
  ../../external/mio/include
  ../../external/fmt/include
  ../../external/json/include
  ../../external/parallel-rdp
  ../../external/parallel-rdp
  ../../external/parallel-rdp/parallel-rdp-standalone/parallel-rdp
  ../../external/parallel-rdp/parallel-rdp-standalone/volk
  ../../external/parallel-rdp/parallel-rdp-standalone/spirv-cross
  ../../external/parallel-rdp/parallel-rdp-standalone/vulkan
  ../../external/parallel-rdp/parallel-rdp-standalone/vulkan-headers/include
  ../../external/parallel-rdp/parallel-rdp-standalone/util
  ../../external/unarr
  ../../external/SDL/include
  ../../external/sse2neon
  ../../external/capstone/include
)

option(RAPIDJSON_BUILD_DOC "Build rapidjson documentation." OFF)
option(RAPIDJSON_BUILD_EXAMPLES "Build rapidjson examples." OFF)
option(RAPIDJSON_BUILD_TESTS "Build rapidjson perftests and unittests." OFF)
option(BUILD_SHARED_LIBS OFF)

include(CheckCCompilerFlag)

check_c_compiler_flag(-msse4.1 HAS_SIMD)
if(NOT HAS_SIMD)
  check_c_compiler_flag(-mfpu=neon HAS_SIMD)
  if(APPLE AND HAS_SIMD)
    set(ARM_APPLE ON)
    message("Compiling on ARM MacOS, very experimental and cool!")
  endif()
endif()

if (HAS_SIMD)
  add_compile_definitions(SIMD_SUPPORT)
  if(ARM_APPLE)
    add_compile_options(-mfpu=neon)
  else()
    add_compile_options(-msse3 -msse4.1)
  endif()
endif ()

if (${CMAKE_BUILD_TYPE} MATCHES Debug)
  add_compile_definitions(VULKAN_DEBUG)
endif ()

add_subdirectory(../../external/discord_rpc discord_rpc)
add_subdirectory(../../external/json json)
add_subdirectory(../../external/fmt fmt)
add_subdirectory(../../external/mio mio)
add_subdirectory(../backend backend)
add_subdirectory(../../external/parallel-rdp parallel-rdp)
add_subdirectory(../../external/unarr unarr)
add_subdirectory(../../external/SDL SDL)
set(CAPSTONE_ARCHITECTURE_DEFAULT OFF)
set(CAPSTONE_MIPS_SUPPORT ON)
set(CAPSTONE_X86_SUPPORT ON)
add_subdirectory(../../external/capstone capstone)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(kaizen-qt
  main.cpp
  KaizenQt.hpp
  KaizenQt.cpp
  RenderWidget.cpp
  RenderWidget.hpp
  EmuThread.hpp
  EmuThread.cpp
  MainWindow.hpp
  MainWindow.cpp
  SettingsWindow.hpp
  SettingsWindow.cpp
  CPUSettings.hpp
  CPUSettings.cpp
  JSONUtils.hpp
  AudioSettings.hpp
  AudioSettings.cpp
  InputSettings.hpp
  InputSettings.cpp
  Debugger.hpp
  Debugger.cpp
  CodeModel.hpp)

target_link_libraries(kaizen-qt PUBLIC SDL3::SDL3 SDL3::SDL3-static Qt6::Core Qt6::Gui Qt6::Widgets discord-rpc fmt mio nlohmann_json parallel-rdp capstone backend)
target_compile_definitions(kaizen-qt PUBLIC SDL_MAIN_HANDLED)

file(COPY ../../resources/ DESTINATION ${PROJECT_BINARY_DIR}/resources/)
file(REMOVE
  ${PROJECT_BINARY_DIR}/resources/mario.png
  ${PROJECT_BINARY_DIR}/resources/shader.frag
  ${PROJECT_BINARY_DIR}/resources/shader.vert)
