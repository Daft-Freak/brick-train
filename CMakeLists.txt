cmake_minimum_required(VERSION 3.13.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

project(brick-train)

if(MSVC)
  add_compile_options("/W4" "/wd4244" "/wd4324" "/wd4458" "/wd4100")
else()
  add_compile_options("-Wall" "-Wextra" "-Wno-unused-parameter")
endif()

add_executable(BrickTrain
  FileLoader.cpp
  IniFile.cpp
  Main.cpp
  Object.cpp
  ObjectData.cpp
  ObjectDataStore.cpp
  ResourceFile.cpp
  RWOps.cpp
  SoundLoader.cpp
  StringTable.cpp
  TextureLoader.cpp
  Train.cpp
  World.cpp
)

find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

target_link_libraries(BrickTrain SDL2::SDL2 SDL2_mixer::SDL2_mixer)

if(SDL2_SDL2main_FOUND)
    target_link_libraries(BrickTrain SDL2::SDL2main)
endif()
