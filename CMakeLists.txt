cmake_minimum_required(VERSION 3.16)

project(OpenGothic LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_DEBUG_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_RPATH ON)

add_library(${PROJECT_NAME} STATIC)
add_executable(Gothic2Notr)
add_executable(Spacer)

if(MSVC)
  add_definitions(-D_USE_MATH_DEFINES)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
  add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")
endif()

if(NOT MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wconversion -Wno-strict-aliasing -Werror)
  if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.0")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-class-memaccess)
  endif()
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7.1" AND NOT APPLE AND NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-format-truncation)
  endif()
  if(UNIX)
    # -rdynamic is needed to allow obtaining meaningful backtraces
    target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # gcc
    # required by pthread_setname_np()
    add_definitions(-D_GNU_SOURCE)
  endif()
endif()

if(WIN32)
  add_definitions(-DNOMINMAX)
  if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup" )
  elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -lpthread")
  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -lstdc++fs")
  endif()
elseif(IOS)
  enable_language(OBJCXX)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0)
  option(OPENGOTHIC_IOS_INCREASED_MEMORY_REQUESTED
         "Mark SideStore test builds as requesting Increased Memory Limit" OFF)
  option(OPENGOTHIC_PERF_DIAGNOSTICS
         "Enable low-frequency performance and memory telemetry" OFF)
  option(OPENGOTHIC_NPC_ANIMATION_CULLING
         "Skip full skeletal pose updates for distant offscreen NPCs" OFF)
  option(OPENGOTHIC_NPC_DIALOG_CULLING
         "Keep distant NPC pose culling active during dialogs and cutscenes" OFF)
  option(OPENGOTHIC_IOS_THREE_FRAMES_IN_FLIGHT
         "Use three frame-resource and Metal swapchain slots on iOS" OFF)
  set(OPENGOTHIC_IOS_METAL_SHADER_CACHE_SIZE "16" CACHE STRING
      "Maximum number of compiled Metal shader modules cached per iOS device; 0 disables the cache")
  if(NOT "${OPENGOTHIC_IOS_METAL_SHADER_CACHE_SIZE}" MATCHES "^(0|[1-9][0-9]*)$")
    message(FATAL_ERROR
      "OPENGOTHIC_IOS_METAL_SHADER_CACHE_SIZE must be a non-negative decimal integer without leading zeroes")
  endif()
  option(OPENGOTHIC_GPU_EXPERIMENT_DIRECT_DRAWABLE_LAZY_SSAO
         "Render directly into CAMetalDrawable and allocate SSAO buffers on demand" OFF)
  option(OPENGOTHIC_GPU_EXPERIMENT_WORLD_FAR_PLANE_60000
         "Limit the iOS world camera far plane to 60000 Gothic units" OFF)
  option(OPENGOTHIC_GPU_EXPERIMENT_DYNAMIC_DRAW_DISTANCE
         "Map the Gothic draw-distance menu setting to the world camera far plane" OFF)
  option(OPENGOTHIC_METALFX_SPATIAL
         "Use MetalFX Spatial for reduced-resolution HDR scene upscaling on supported iOS devices" OFF)
  option(OPENGOTHIC_METALFX_TEMPORAL
         "Use MetalFX Temporal AA upscaling with Spatial/Lanczos fallback on supported iOS devices" OFF)
  option(OPENGOTHIC_IOS_PRECOMPILED_STARTUP_SHADERS
         "Bundle a small precompiled Metal library for the first iOS menu/video shaders" ON)
  # App version. CI passes -DOPENGOTHIC_IOS_VERSION=1.0.<run> so the bundle's
  # CFBundleShortVersionString matches the "version" field in apps.json —
  # otherwise SideStore/AltStore rejects the download as a version mismatch.
  if(NOT DEFINED OPENGOTHIC_IOS_VERSION)
    set(OPENGOTHIC_IOS_VERSION "1.0")
  endif()
  set(OPENGOTHIC_IOS_BUNDLE_IDENTIFIER "opengothic.gothic2" CACHE STRING
      "Bundle identifier used for the iOS application")
  set(OPENGOTHIC_IOS_DISPLAY_NAME "OpenGothic" CACHE STRING
      "Display name used for the iOS application")
  set(OPENGOTHIC_IOS_DEVELOPMENT_TEAM "" CACHE STRING
      "Apple development team used for local iOS device builds")
  set_directory_properties("${CMAKE_SOURCE_DIR}" PROPERTIES
    PROPERTY XCODE_STARTUP_PROJECT Gothic2Notr)

  # OpenGothic is a static library.  All bundle and signing metadata therefore
  # belongs to the runnable Gothic2Notr target, not to the library.
  set_target_properties(Gothic2Notr PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${OPENGOTHIC_IOS_BUNDLE_IDENTIFIER}"
    MACOSX_BUNDLE_BUNDLE_NAME "${OPENGOTHIC_IOS_DISPLAY_NAME}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${OPENGOTHIC_IOS_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${OPENGOTHIC_IOS_VERSION}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "OpenGothic ${OPENGOTHIC_IOS_VERSION}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${OPENGOTHIC_IOS_BUNDLE_IDENTIFIER}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
  if(OPENGOTHIC_IOS_DEVELOPMENT_TEAM)
    set_target_properties(Gothic2Notr PROPERTIES
      XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${OPENGOTHIC_IOS_DEVELOPMENT_TEAM}"
      XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
  endif()
  if(OPENGOTHIC_IOS_INCREASED_MEMORY_REQUESTED)
    set_target_properties(Gothic2Notr PROPERTIES
      XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
        "${CMAKE_SOURCE_DIR}/ios/OpenGothic.entitlements")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      OPENGOTHIC_IOS_INCREASED_MEMORY_REQUESTED=1)
  endif()
  if(OPENGOTHIC_PERF_DIAGNOSTICS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      OPENGOTHIC_PERF_DIAGNOSTICS=1)
  endif()
  if(OPENGOTHIC_NPC_ANIMATION_CULLING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      OPENGOTHIC_NPC_ANIMATION_CULLING=1)
  endif()
  if(OPENGOTHIC_NPC_DIALOG_CULLING)
    if(NOT OPENGOTHIC_NPC_ANIMATION_CULLING)
      message(FATAL_ERROR "OPENGOTHIC_NPC_DIALOG_CULLING requires OPENGOTHIC_NPC_ANIMATION_CULLING")
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      OPENGOTHIC_NPC_DIALOG_CULLING=1)
  endif()
  if(OPENGOTHIC_IOS_THREE_FRAMES_IN_FLIGHT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      OPENGOTHIC_IOS_THREE_FRAMES_IN_FLIGHT=1)
    target_compile_definitions(Gothic2Notr PRIVATE
      OPENGOTHIC_IOS_THREE_FRAMES_IN_FLIGHT=1)
  endif()
  target_compile_definitions(Gothic2Notr PRIVATE
    OPENGOTHIC_IOS_METAL_SHADER_CACHE_SIZE=${OPENGOTHIC_IOS_METAL_SHADER_CACHE_SIZE})
  if(OPENGOTHIC_GPU_EXPERIMENT_DIRECT_DRAWABLE_LAZY_SSAO)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      OPENGOTHIC_GPU_EXPERIMENT_DIRECT_DRAWABLE_LAZY_SSAO=1)
  endif()
  if(OPENGOTHIC_GPU_EXPERIMENT_WORLD_FAR_PLANE_60000 AND
     OPENGOTHIC_GPU_EXPERIMENT_DYNAMIC_DRAW_DISTANCE)
    message(FATAL_ERROR "Fixed and dynamic world far-plane experiments are mutually exclusive")
  endif()
  if(OPENGOTHIC_GPU_EXPERIMENT_WORLD_FAR_PLANE_60000)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      OPENGOTHIC_GPU_EXPERIMENT_WORLD_FAR_PLANE_60000=1)
  endif()
  if(OPENGOTHIC_GPU_EXPERIMENT_DYNAMIC_DRAW_DISTANCE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      OPENGOTHIC_GPU_EXPERIMENT_DYNAMIC_DRAW_DISTANCE=1)
  endif()
  if(OPENGOTHIC_METALFX_SPATIAL)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      OPENGOTHIC_METALFX_SPATIAL=1)
  endif()
  if(OPENGOTHIC_METALFX_TEMPORAL)
    if(NOT OPENGOTHIC_METALFX_SPATIAL)
      message(FATAL_ERROR "OPENGOTHIC_METALFX_TEMPORAL requires OPENGOTHIC_METALFX_SPATIAL for its fallback path")
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      OPENGOTHIC_METALFX_TEMPORAL=1)
  endif()
  # App icon: Xcode's actool compiles this asset catalog on the CI runner and
  # injects the CFBundleIcons keys, so the icon shows on the home screen.
  set(OPENGOTHIC_APPICON "${CMAKE_SOURCE_DIR}/ios/Assets.xcassets")
  target_sources(Gothic2Notr PRIVATE "${OPENGOTHIC_APPICON}")
  set_source_files_properties("${OPENGOTHIC_APPICON}" PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
elseif(APPLE)
  enable_language(OBJCXX)
endif()

if(IOS)
  file(GLOB_RECURSE ObjCSOURCES "common/*.mm")
  # The framework references are emitted by objects in the static OpenGothic
  # library.  Propagate them to every final executable that consumes it.
  target_link_libraries(${PROJECT_NAME}
    "-framework UIKit"
    "-framework GameController"
    "-framework AVFoundation"
    "-framework Security")
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} "-framework AppKit")
  file(GLOB_RECURSE ObjCSOURCES "common/*.mm")
endif()

## OpenGothic

# sources
file(GLOB_RECURSE OPENGOTHIC_SOURCES "common/*.h" "common/*.cpp")
file(GLOB_RECURSE SPACER_SOURCES "spacer/*.h" "spacer/*.cpp")

target_sources(${PROJECT_NAME} PRIVATE ${OPENGOTHIC_SOURCES} ${ObjCSOURCES})

# Bundle the CC0 controller glyphs (Xelu prompts) into the iOS app.
if(IOS)
  file(GLOB CONTROLLER_GLYPHS "${CMAKE_SOURCE_DIR}/assets/controller/*.png")
  set_source_files_properties(${CONTROLLER_GLYPHS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  target_sources(Gothic2Notr PRIVATE ${CONTROLLER_GLYPHS})
endif()

# shaders
add_subdirectory(shader)
target_link_libraries(${PROJECT_NAME} GothicShaders)

# editor icons and such
add_subdirectory(spacer/assets)

include_directories("common")

if(IOS AND OPENGOTHIC_IOS_PRECOMPILED_STARTUP_SHADERS AND
   OPENGOTHIC_IOS_STARTUP_METALLIB)
  set_source_files_properties("${OPENGOTHIC_IOS_STARTUP_METALLIB}" PROPERTIES
    GENERATED TRUE
    MACOSX_PACKAGE_LOCATION Resources)
  target_sources(Gothic2Notr PRIVATE "${OPENGOTHIC_IOS_STARTUP_METALLIB}")
endif()

## submodule dependencies

# edd-dbg
if(WIN32)
  add_subdirectory(lib/edd-dbg)
  include_directories(lib/edd-dbg/include)
  target_link_libraries(${PROJECT_NAME} edd_dbg)
endif()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} shlwapi DbgHelp)
elseif(UNIX)
  target_link_libraries(${PROJECT_NAME} -lpthread -ldl)
endif()

# ZenKit
set(BUILD_SQUISH_WITH_SSE2     OFF CACHE INTERNAL "")
set(ZK_ENABLE_ASAN             OFF CACHE INTERNAL "")
set(ZK_ENABLE_DEPRECATION      OFF CACHE INTERNAL "")

add_subdirectory(lib/ZenKit)
target_link_libraries(${PROJECT_NAME} zenkit)

# dmusic
set(DM_BUILD_STATIC            ON  CACHE INTERNAL "")
set(DM_ENABLE_ASAN             OFF CACHE INTERNAL "")

add_subdirectory(lib/dmusic)
target_link_libraries(${PROJECT_NAME} dmusic)

# TinySoundFont
add_definitions(-DTSF_NO_STDIO)
include_directories(lib/TinySoundFont)

# Rapidjson
include_directories(lib/rapidjson/include)

# Tempest
if(WIN32)
  set(TEMPEST_BUILD_SHARED ON  CACHE INTERNAL "")
else()
  set(TEMPEST_BUILD_SHARED OFF CACHE INTERNAL "")
endif()
add_subdirectory(lib/Tempest/Engine)
if(IOS AND OPENGOTHIC_IOS_PRECOMPILED_STARTUP_SHADERS AND
   OPENGOTHIC_IOS_STARTUP_METALLIB)
  target_compile_definitions(Tempest PRIVATE
    TEMPEST_METAL_STARTUP_LIBRARY="OpenGothicStartup.metallib")
endif()
if(IOS AND OPENGOTHIC_GPU_EXPERIMENT_DIRECT_DRAWABLE_LAZY_SSAO)
  target_compile_definitions(Tempest PRIVATE TEMPEST_METAL_DIRECT_DRAWABLE=1)
endif()
include_directories(lib/Tempest/Engine/include)
target_link_libraries(${PROJECT_NAME} Tempest)

# zip
include_directories(lib/miniz)
add_definitions(-DMINIZ_NO_STDIO)
add_library(miniz STATIC EXCLUDE_FROM_ALL "lib/miniz/miniz.h" "lib/miniz/miniz.c")
target_link_libraries(${PROJECT_NAME} miniz)

# bullet physics
set(BULLET2_MULTITHREADING       ON  CACHE INTERNAL "")
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON  CACHE INTERNAL "")
set(USE_GRAPHICAL_BENCHMARK      OFF CACHE INTERNAL "")
set(BUILD_BULLET2_DEMOS          OFF CACHE INTERNAL "") # No samples
set(BUILD_OPENGL3_DEMOS          OFF CACHE INTERNAL "") # No samples
set(BUILD_UNIT_TESTS             OFF CACHE INTERNAL "") # No tests
set(BUILD_CPU_DEMOS              OFF CACHE INTERNAL "") # No samples
set(BUILD_EXTRAS                 OFF CACHE INTERNAL "") # No bugs
set(GLFW_BUILD_EXAMPLES          OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS             OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS              OFF CACHE INTERNAL "")
set(BUILD_BULLET3                OFF CACHE INTERNAL "") # Can use bullet2, bullet3 wants to build examples...
add_subdirectory(lib/bullet3)
if(NOT MSVC)
  target_compile_options(BulletDynamics         PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletInverseDynamics  PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletSoftBody         PRIVATE -Wno-argument-outside-range)
  target_compile_options(Bullet3Common          PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletCollision        PRIVATE -Wno-argument-outside-range)
  target_compile_options(LinearMath             PRIVATE -Wno-argument-outside-range)
endif()
if(NOT MSVC AND NOT APPLE AND NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  target_compile_options(BulletDynamics         PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletInverseDynamics  PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletSoftBody         PRIVATE -Wno-stringop-overflow)
  target_compile_options(Bullet3Common          PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletCollision        PRIVATE -Wno-stringop-overflow)
  target_compile_options(LinearMath             PRIVATE -Wno-stringop-overflow)
endif()

include_directories(lib/bullet3/src)
target_link_libraries(${PROJECT_NAME} BulletDynamics BulletCollision LinearMath)

# script for launching in binary directory
if(WIN32)
    add_custom_command(
        TARGET Gothic2Notr POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/scripts/Gothic2Notr.bat
        ${CMAKE_CURRENT_BINARY_DIR}/opengothic/Gothic2Notr.bat)
endif()

# in debug mode, enable sanitizers
if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  add_compile_options(-fsanitize=address)
  add_compile_options(-fsanitize=leak)
  add_link_options   (-fsanitize=address)
  add_link_options   (-fsanitize=leak)
endif()

# the game
target_sources(Gothic2Notr PRIVATE game/main.cpp icon.rc)
target_link_libraries(Gothic2Notr PRIVATE OpenGothic)

# editor
target_sources(Spacer PRIVATE ${SPACER_SOURCES} spacer/main.cpp)
target_include_directories(Spacer PRIVATE "spacer")
target_link_libraries(Spacer PRIVATE OpenGothic SpacerAssets)

# installation
install(TARGETS Gothic2Notr DESTINATION bin)
