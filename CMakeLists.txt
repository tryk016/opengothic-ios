cmake_minimum_required(VERSION 3.16)

project(Gothic2Notr LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_DEBUG_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_RPATH ON)

if(ANDROID)
  add_library(${PROJECT_NAME} SHARED)
else()
  add_executable(${PROJECT_NAME})
endif()

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
  if(UNIX AND NOT ANDROID)
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
         "Use three OpenGothic frame-resource slots on iOS" OFF)
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
  # App version. CI passes -DOPENGOTHIC_IOS_VERSION=1.0.<run> so the bundle's
  # CFBundleShortVersionString matches the "version" field in apps.json —
  # otherwise SideStore/AltStore rejects the download as a version mismatch.
  if(NOT DEFINED OPENGOTHIC_IOS_VERSION)
    set(OPENGOTHIC_IOS_VERSION "1.0")
  endif()
  set_directory_properties("${CMAKE_SOURCE_DIR}" PROPERTIES
    PROPERTY XCODE_STARTUP_PROJECT "${PROJECT_NAME}")
  set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "opengothic.gothic2"
    MACOSX_BUNDLE_BUNDLE_NAME "OpenGothic"
    MACOSX_BUNDLE_BUNDLE_VERSION "${OPENGOTHIC_IOS_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${OPENGOTHIC_IOS_VERSION}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
  if(OPENGOTHIC_IOS_INCREASED_MEMORY_REQUESTED)
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
  endif()
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
  target_sources(${PROJECT_NAME} PRIVATE "${OPENGOTHIC_APPICON}")
  set_source_files_properties("${OPENGOTHIC_APPICON}" PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
elseif(APPLE)
  enable_language(OBJCXX)
elseif(ANDROID)
  # GameActivity native glue
  find_package(game-activity REQUIRED CONFIG)
  # game/main_android.cpp includes Tempest-internal headers (e.g.
  # "system/api/androidapi.h") by path relative to Engine/, which is not on
  # the public include path added below (Engine/include only).
  include_directories(lib/Tempest/Engine)
endif()

if(IOS)
  file(GLOB_RECURSE ObjCSOURCES "game/*.mm")
  target_link_libraries(${PROJECT_NAME} "-framework UIKit" "-framework GameController" "-framework AVFoundation" "-framework Security")
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} "-framework AppKit")
  file(GLOB_RECURSE ObjCSOURCES "game/*.mm")
endif()

if(ANDROID)
  # Android entry lives in game/main_android.cpp; .mm files are Apple-only and excluded by the glob (only *.cpp/*.h gathered).
  # GameActivity's JNI entry points (Java_..._initializeNativeCode etc.) are only
  # called from Java, so Release --gc-sections strips them from libGothic2Notr.so
  # -> java.lang.UnsatisfiedLinkError at startup. --whole-archive keeps the glue.
  target_link_libraries(${PROJECT_NAME}
    -Wl,--whole-archive
    game-activity::game-activity_static
    -Wl,--no-whole-archive
    android log)
endif()

## OpenGothic

# sources
file(GLOB_RECURSE OPENGOTHIC_SOURCES
    "game/*.h"
    "game/*.cpp")

target_sources(${PROJECT_NAME} PRIVATE ${OPENGOTHIC_SOURCES} ${ObjCSOURCES} icon.rc)

# Bundle the CC0 controller glyphs (Xelu prompts) into the iOS app.
if(IOS)
  file(GLOB CONTROLLER_GLYPHS "${CMAKE_SOURCE_DIR}/assets/controller/*.png")
  set_source_files_properties(${CONTROLLER_GLYPHS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  target_sources(${PROJECT_NAME} PRIVATE ${CONTROLLER_GLYPHS})
endif()

# shaders
add_subdirectory(shader)
target_link_libraries(${PROJECT_NAME} GothicShaders)

include_directories("game")

## submodule dependencies

# edd-dbg
if(WIN32)
  add_subdirectory(lib/edd-dbg)
  include_directories(lib/edd-dbg/include)
  target_link_libraries(${PROJECT_NAME} edd_dbg)
endif()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} shlwapi DbgHelp)
elseif(ANDROID)
  # nothing extra here; handled above
elseif(UNIX)
  target_link_libraries(${PROJECT_NAME} -lpthread -ldl)
endif()

# ZenKit
set(BUILD_SQUISH_WITH_SSE2     OFF CACHE INTERNAL "")
set(ZK_ENABLE_ASAN             OFF CACHE INTERNAL "")
set(ZK_ENABLE_DEPRECATION      OFF CACHE INTERNAL "")

add_subdirectory(lib/ZenKit)
target_link_libraries(${PROJECT_NAME} zenkit)

# astcenc — DXT->ASTC transcoding for GPUs without BC/S3TC.
# Such GPUs make Tempest decompress every DXT texture to RGBA8 (4-8x memory);
# ASTC keeps them compressed at full resolution. Android-only for now; desktop
# samples DXT natively and never enters that path.
# NOTE: mobile Vulkan essentially has no BC at all -- Phase 1 measured DXT1=0 on
# BOTH Mali-G57 and Adreno 619, so this is not a Mali-specific workaround.
# See docs/superpowers/specs/2026-07-16-astc-transcoder-design.md
if(ANDROID)
  set(ASTCENC_CLI            OFF CACHE INTERNAL "") # library only, no command-line tool
  set(ASTCENC_SHAREDLIB      OFF CACHE INTERNAL "")
  set(ASTCENC_UNITTEST       OFF CACHE INTERNAL "")
  set(ASTCENC_DECOMPRESSOR   OFF CACHE INTERNAL "") # we encode, so build the full codec
  set(ASTCENC_DIAGNOSTICS    OFF CACHE INTERNAL "")
  set(ASTCENC_WERROR         OFF CACHE INTERNAL "") # astcenc defaults this ON; don't let its
                                                    # warnings under the NDK's clang fail our build
  set(ASTCENC_ISA_NATIVE     OFF CACHE INTERNAL "")
  set(ASTCENC_ISA_NONE       OFF CACHE INTERNAL "")

  # Exactly one ISA must be enabled, and it must match the ABI being built:
  # abiFilters is 'arm64-v8a', 'x86_64' (android/app/build.gradle), and NEON
  # does not exist on x86_64. The target name follows astcenc's own
  # astc${CODEC}-${ISA}-static scheme (Source/cmake_core.cmake:18).
  if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(ASTCENC_ISA_NEON     ON  CACHE INTERNAL "")
    set(ASTCENC_LIB astcenc-neon-static)
  elseif(ANDROID_ABI STREQUAL "x86_64")
    set(ASTCENC_ISA_SSE41    ON  CACHE INTERNAL "")
    set(ASTCENC_LIB astcenc-sse4.1-static)
  else()
    message(FATAL_ERROR "astcenc: unhandled ANDROID_ABI '${ANDROID_ABI}'")
  endif()

  add_subdirectory(lib/astcenc)
  target_include_directories(${PROJECT_NAME} PRIVATE lib/astcenc/Source)
  target_link_libraries(${PROJECT_NAME} ${ASTCENC_LIB})
  # Guards game/graphics/astctranscoder.* and its call site. Keyed on the encoder being
  # linked in, not on the platform: the transcoder itself gates on GPU capability, so the
  # iOS port only has to define this and build astcenc.
  target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_ASTCENC=1)
endif()

# dmusic
set(DM_BUILD_STATIC            ON  CACHE INTERNAL "")
set(DM_ENABLE_ASAN             OFF CACHE INTERNAL "")

add_subdirectory(lib/dmusic)
target_link_libraries(${PROJECT_NAME} dmusic)

# TinySoundFont
add_definitions(-DTSF_NO_STDIO)
include_directories(lib/TinySoundFont)

# Tempest
if(WIN32)
  set(TEMPEST_BUILD_SHARED ON  CACHE INTERNAL "")
else()
  set(TEMPEST_BUILD_SHARED OFF CACHE INTERNAL "")
endif()
add_subdirectory(lib/Tempest/Engine)
if(IOS AND OPENGOTHIC_GPU_EXPERIMENT_DIRECT_DRAWABLE_LAZY_SSAO)
  target_compile_definitions(Tempest PRIVATE TEMPEST_METAL_DIRECT_DRAWABLE=1)
endif()
include_directories(lib/Tempest/Engine/include)
target_link_libraries(${PROJECT_NAME} Tempest)

# zip
include_directories(lib/miniz)
add_definitions(-DMINIZ_NO_STDIO)
add_library(miniz STATIC "lib/miniz/miniz.h" "lib/miniz/miniz.c")
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
        TARGET ${PROJECT_NAME} POST_BUILD
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

# installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
