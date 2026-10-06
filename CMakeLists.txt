cmake_minimum_required(VERSION 3.24)
project(GameEnginePortable LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/utf-8)
endif()

# The existing Visual Studio editor remains available. This target builds the
# native rendering preview and its GPU backends on Windows, macOS and Linux.
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

# The native renderer preview and Windows editor use the same application entry files.
# Scene/gameplay and editor portability are still pending; this target builds the GPU foundation.
add_executable(GameEnginePortable
  Source/main.cpp
  Source/GameApplication/gameApplication.cpp
  Source/GameApplication/Service/Graphics/RHI/SDL/SDLGPUBackend.cpp
  Source/GameApplication/Service/Graphics/Portable/FrameRenderer.cpp)
target_include_directories(GameEnginePortable PRIVATE Source/GameApplication)
target_compile_definitions(GameEnginePortable PRIVATE GAMEENGINE_PORTABLE)
target_link_libraries(GameEnginePortable PRIVATE SDL3::SDL3)
if(MSVC)
  target_compile_options(GameEnginePortable PRIVATE /W4 /permissive-)
else()
  target_compile_options(GameEnginePortable PRIVATE -Wall -Wextra -Wpedantic)
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
