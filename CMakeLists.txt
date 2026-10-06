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
  Source/GameApplication/Engine/Resources/Data/modelData.cpp
  Source/GameApplication/Service/Graphics/RHI/SDL/SDLGPUBackend.cpp
  Source/GameApplication/Service/Graphics/Portable/FrameRenderer.cpp)
target_include_directories(GameEnginePortable PRIVATE
  Source Source/GameApplication Source/GameApplication/Engine Source/GameApplication/Engine/Scene)
target_compile_definitions(GameEnginePortable PRIVATE GAMEENGINE_PORTABLE)
target_link_libraries(GameEnginePortable PRIVATE SDL3::SDL3)
# Use the existing ModelLoader and ModelData with the engine's Assimp version.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(CMAKE_POLICY_DEFAULT_CMP0175 OLD)
FetchContent_Declare(assimp
  URL https://codeload.github.com/assimp/assimp/tar.gz/refs/tags/v5.2.0
  URL_HASH SHA256=18723edbea249ea11e02f62e040869e6a733cd2e969e96da02c101c62677f72d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(assimp)
if(MSVC)
  target_compile_options(assimp PRIVATE /WX-)
else()
  target_compile_options(assimp PRIVATE -include stddef.h -Wno-error)
endif()
target_link_libraries(GameEnginePortable PRIVATE assimp)
install(FILES "${assimp_SOURCE_DIR}/LICENSE" DESTINATION licenses RENAME Assimp-LICENSE.txt)
install(FILES "${assimp_SOURCE_DIR}/contrib/zlib/README" DESTINATION licenses RENAME Zlib-README.txt)
# Reuse the engine's DirectXMath 3.19 types on native non-Windows builds.
# Headers only: do not bring in DirectXMath's own build/install/test targets.
if(NOT WIN32)
  include(FetchContent)
  FetchContent_Declare(directxmath
    URL https://codeload.github.com/microsoft/DirectXMath/tar.gz/58f045a6caaaad6ce15049bba3cd075ed419c85e
    URL_HASH SHA256=ebe7943f3078a53bf38214b7a5b81ab83007615de3d59fa730d673fa1873159a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_SUBDIR Inc)
  FetchContent_MakeAvailable(directxmath)
  set(sal_include "${CMAKE_CURRENT_BINARY_DIR}/directxmath-sal")
  file(MAKE_DIRECTORY "${sal_include}")
  file(DOWNLOAD
    https://raw.githubusercontent.com/dotnet/corert/9d8bd29a71f68941aaa2c00e9c9a0eed86cafa10/src/Native/inc/unix/sal.h
    "${sal_include}/sal.h" TLS_VERIFY ON
    EXPECTED_HASH SHA256=bb9ea7c1d05cdfc10a17fe80d87a7cd160c1063c7e5fa089cd6405c31c610a1b)
  file(DOWNLOAD
    https://raw.githubusercontent.com/dotnet/corert/9d8bd29a71f68941aaa2c00e9c9a0eed86cafa10/LICENSE.TXT
    "${sal_include}/SAL-LICENSE.txt" TLS_VERIFY ON
    EXPECTED_HASH SHA256=18647d26bc4c4d892a5fd29de1264d10906b3124cafa98313300ff300441c7f0)
  target_include_directories(GameEnginePortable SYSTEM PRIVATE "${directxmath_SOURCE_DIR}/Inc" "${sal_include}")
  # SAL's legacy __null annotation must not replace Clang's C++ NULL value.
  target_compile_definitions(GameEnginePortable PRIVATE PAL_STDCPP_COMPAT)
  install(FILES "${directxmath_SOURCE_DIR}/LICENSE" DESTINATION licenses RENAME DirectXMath-LICENSE.txt)
  install(FILES "${sal_include}/SAL-LICENSE.txt" DESTINATION licenses)
endif()
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
