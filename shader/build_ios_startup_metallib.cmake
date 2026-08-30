cmake_minimum_required(VERSION 3.16)

foreach(REQUIRED_VAR
    STARTUP_DIR STARTUP_METALLIB TRIANGLE_SPV DOWNSCALE_SPV
    SPIRV_CROSS XCRUN DEPLOYMENT_TARGET METAL_SDK METAL_TARGET)
  if(NOT DEFINED ${REQUIRED_VAR} OR "${${REQUIRED_VAR}}" STREQUAL "")
    message(FATAL_ERROR "Missing ${REQUIRED_VAR} for the iOS startup metallib")
  endif()
endforeach()

file(MAKE_DIRECTORY "${STARTUP_DIR}")

function(compile_startup_shader NAME STAGE SPV)
  file(SHA256 "${SPV}" SPV_SHA256)
  string(SUBSTRING "${SPV_SHA256}" 0 16 SPV_ID)
  set(ENTRY_POINT "og_${SPV_ID}")
  set(METAL_SOURCE "${STARTUP_DIR}/${NAME}.metal")
  set(AIR_OBJECT "${STARTUP_DIR}/${NAME}.air")

  execute_process(
    COMMAND "${SPIRV_CROSS}" "${SPV}"
      --msl --msl-ios --msl-version 20400 --flip-vert-y
      --rename-entry-point main "${ENTRY_POINT}" "${STAGE}"
      --output "${METAL_SOURCE}"
    RESULT_VARIABLE CROSS_RESULT
    ERROR_VARIABLE CROSS_ERROR)
  if(NOT CROSS_RESULT EQUAL 0)
    message(FATAL_ERROR "SPIRV-Cross failed for ${SPV}: ${CROSS_ERROR}")
  endif()

  execute_process(
    COMMAND "${XCRUN}" -sdk "${METAL_SDK}" metal
      -std=ios-metal2.4 -target "${METAL_TARGET}"
      -c "${METAL_SOURCE}" -o "${AIR_OBJECT}"
    RESULT_VARIABLE METAL_RESULT
    ERROR_VARIABLE METAL_ERROR)
  if(NOT METAL_RESULT EQUAL 0)
    message(FATAL_ERROR "Metal compilation failed for ${METAL_SOURCE}: ${METAL_ERROR}")
  endif()
endfunction()

compile_startup_shader(triangle vert "${TRIANGLE_SPV}")
compile_startup_shader(downscale frag "${DOWNSCALE_SPV}")

execute_process(
  COMMAND "${XCRUN}" -sdk "${METAL_SDK}" metallib
    "${STARTUP_DIR}/triangle.air"
    "${STARTUP_DIR}/downscale.air"
    -o "${STARTUP_METALLIB}"
  RESULT_VARIABLE METALLIB_RESULT
  ERROR_VARIABLE METALLIB_ERROR)
if(NOT METALLIB_RESULT EQUAL 0)
  message(FATAL_ERROR "metallib link failed: ${METALLIB_ERROR}")
endif()
