cmake_minimum_required(VERSION 3.23)
project(File_System)

set(CMAKE_CXX_STANDARD 20)

set(raylib_dir "C:\\Users\\aamir\\CLionProjects\\libraries\\raylib-4.2.0")

include_directories(${raylib_dir}\\include)
link_directories(${raylib_dir}\\lib)


add_executable(${PROJECT_NAME} main.cpp FileSystem.cpp FileSystem.h)
target_link_libraries(${PROJECT_NAME} raylib winmm gdi32 opengl32)
