cmake_minimum_required(VERSION 3.10)

project(LevelRenderer_OpenGL)

# currently using unicode in some libraries on win32 but will change soon
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

if (UNIX AND NOT APPLE)
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lX11 -lGL")
     find_package(X11)
     find_package(OpenGL)
     include_directories(${X11_INCLUDE_DIR})
     link_libraries(${X11_LIBRARIES})
     include_directories(${OPENGL_INCLUDE_DIRS})
     link_libraries(${OPENGL_LIBRARIES})
endif()

add_executable (LevelRenderer_OpenGL main.cpp renderer.h h2bParser.h)

