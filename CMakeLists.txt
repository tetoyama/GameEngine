cmake_minimum_required(VERSION 3.24)
project(GameEnginePortable LANGUAGES CXX)
set(GAMEENGINE_TARGET_PLATFORM "${CMAKE_SYSTEM_NAME}" CACHE STRING "Package target: Windows, macOS or Linux")
if(GAMEENGINE_TARGET_PLATFORM STREQUAL "macOS")
  set(expected_system Darwin)
else()
  set(expected_system "${GAMEENGINE_TARGET_PLATFORM}")
endif()
if(NOT expected_system STREQUAL CMAKE_SYSTEM_NAME)
  message(FATAL_ERROR "Target ${GAMEENGINE_TARGET_PLATFORM} requires its native host/toolchain. This toolchain builds ${CMAKE_SYSTEM_NAME}.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/utf-8)
endif()

# The existing Visual Studio editor remains available. This target builds the
# platform-independent runtime and its GPU backends on Windows, macOS and Linux.
find_package(SDL3 3.4 CONFIG QUIET)
if(NOT SDL3_FOUND)
  include(FetchContent)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
    URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

add_library(GameEngineCore STATIC
  Source/GameApplication/Engine/engineContextLifecycle.cpp
  Source/GameApplication/Service/Runtime/TimeService/timeService.cpp)
target_include_directories(GameEngineCore PUBLIC Source Source/GameApplication)

add_library(GameEngineRHI STATIC
  Source/GameApplication/Service/Graphics/RHI/SDL/SDLGPUBackend.cpp
  Source/GameApplication/Service/Graphics/Portable/FrameRenderer.cpp)
target_include_directories(GameEngineRHI PUBLIC Source/GameApplication)
target_link_libraries(GameEngineRHI PUBLIC SDL3::SDL3)
if(MSVC)
  target_compile_options(GameEngineRHI PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(GameEngineRHI PRIVATE -Wall -Wextra -Wpedantic)
endif()
option(BUILD_TESTING "Build engine contract tests" ON)
if(BUILD_TESTING)
  enable_testing()
  add_executable(EngineLifecycleContractTest Tests/EngineLifecycleContractTest.cpp)
  target_link_libraries(EngineLifecycleContractTest PRIVATE GameEngineCore)
  add_test(NAME EngineLifecycleContract COMMAND EngineLifecycleContractTest)
  add_executable(PortableRenderPacketContractTest Tests/PortableRenderPacketContractTest.cpp)
  target_include_directories(PortableRenderPacketContractTest PRIVATE
    Source/GameApplication Source/GameApplication/Engine Source/GameApplication/Engine/Scene)
  add_test(NAME PortableRenderPacketContract COMMAND PortableRenderPacketContractTest)
  if(MSVC)
    add_executable(EngineGraphicsConfigTest Tests/EngineGraphicsConfigTest.cpp)
    target_include_directories(EngineGraphicsConfigTest PRIVATE Source Source/GameApplication
      Source/GameApplication/Engine Source/GameApplication/Engine/Scene Source/GameApplication/Service Source/GameApplication/Backends)
    target_link_directories(EngineGraphicsConfigTest PRIVATE Source/GameApplication/Backends/yaml-cpp)
    add_test(NAME EngineGraphicsConfig COMMAND EngineGraphicsConfigTest)
  endif()
  foreach(test JobSystemSmokeTest RHISmokeTest)
    add_executable(${test} Tests/${test}.cpp)
    target_include_directories(${test} PRIVATE Source/GameApplication Source/GameApplication/Engine Source/GameApplication/Engine/Scene)
    add_test(NAME ${test} COMMAND ${test})
    if(MSVC)
      target_compile_options(${test} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${test} PRIVATE -UNDEBUG)
    endif()
  endforeach()
endif()
add_executable(GameEnginePortable Source/GameApplication/Portable/PortableMain.cpp)
target_include_directories(GameEnginePortable PRIVATE
  Source/GameApplication/Engine Source/GameApplication/Engine/Scene)
target_link_libraries(GameEnginePortable PRIVATE GameEngineRHI GameEngineCore)
if(MSVC)
  target_compile_options(GameEnginePortable PRIVATE /W4 /permissive- /utf-8)
endif()
add_custom_command(TARGET GameEnginePortable POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/Shader/Portable/Compiled"
    "$<TARGET_FILE_DIR:GameEnginePortable>/shaders")
get_target_property(SDL_LIBRARY_TYPE SDL3::SDL3 TYPE)
if(WIN32 AND SDL_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
  add_custom_command(TARGET GameEnginePortable POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:GameEnginePortable>")
endif()
option(GAMEENGINE_GPU_TESTS "Enable manual real-GPU renderer validation" OFF)
if(BUILD_TESTING AND GAMEENGINE_GPU_TESTS)
  add_executable(SDLGPUContractTest Tests/SDLGPUContractTest.cpp)
  target_link_libraries(SDLGPUContractTest PRIVATE GameEngineRHI)
  if(WIN32 AND SDL_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET SDLGPUContractTest POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:SDLGPUContractTest>")
  endif()
  if(WIN32)
    set(gpu_backends d3d12 vulkan)
    add_executable(EditorGPUViewportTest Tests/EditorGPUViewportTest.cpp)
    target_include_directories(EditorGPUViewportTest PRIVATE Source/GameApplication/Engine Source/GameApplication/Engine/Scene)
    target_link_libraries(EditorGPUViewportTest PRIVATE GameEngineRHI d3d11)
    if(SDL_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
      add_custom_command(TARGET EditorGPUViewportTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:EditorGPUViewportTest>")
    endif()
  elseif(APPLE)
    set(gpu_backends metal)
  else()
    set(gpu_backends vulkan)
  endif()
  foreach(backend IN LISTS gpu_backends)
    add_test(NAME Renderer_${backend}
      COMMAND GameEnginePortable --backend ${backend} --validate --offscreen
        --width 480 --height 320 --capture "${CMAKE_CURRENT_BINARY_DIR}/captures/${backend}.ppm")
    set_tests_properties(Renderer_${backend} PROPERTIES TIMEOUT 90 LABELS "gpu;manual")
    add_test(NAME GPUContract_${backend} COMMAND SDLGPUContractTest ${backend} "${CMAKE_CURRENT_SOURCE_DIR}/Asset/Shader/Portable/Compiled")
    set_tests_properties(GPUContract_${backend} PROPERTIES TIMEOUT 90 LABELS "gpu;manual")
    if(WIN32)
      add_test(NAME EditorViewport_${backend} COMMAND EditorGPUViewportTest ${backend})
      set_tests_properties(EditorViewport_${backend} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 90 LABELS "gpu;manual")
    endif()
  endforeach()
endif()
install(TARGETS GameEnginePortable RUNTIME DESTINATION bin)
install(DIRECTORY Asset/Shader/Portable/Compiled/ DESTINATION bin/shaders)
if(WIN32 AND SDL_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
  install(FILES "$<TARGET_FILE:SDL3::SDL3>" DESTINATION bin)
endif()
install(FILES ThirdParty/SDL3-LICENSE.txt DESTINATION licenses)
find_program(PORTABLE_GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
find_program(PORTABLE_SPIRV_CROSS spirv-cross HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
find_program(PORTABLE_DXC dxc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(PORTABLE_GLSLC AND PORTABLE_SPIRV_CROSS AND PORTABLE_DXC)
  add_custom_target(PortableShaders
    COMMAND ${CMAKE_COMMAND}
      "-DGLSLC=${PORTABLE_GLSLC}" "-DSPIRV_CROSS=${PORTABLE_SPIRV_CROSS}" "-DDXC=${PORTABLE_DXC}"
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/Asset/Shader/Portable"
      "-DOUTPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR}/Asset/Shader/Portable/Compiled"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilePortableShaders.cmake"
    COMMENT "Compile common shaders for Vulkan, Direct3D 12 and Metal")
endif()
