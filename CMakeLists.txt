cmake_minimum_required(VERSION 3.10) # 3.14 handles PIC differently
project (clippy C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(lib-src
  lib.cpp
  simple-raw.cpp
  )
set(remote-src
  remote.cpp)
set(local-src
  local.cpp)

add_library(${PROJECT_NAME}lib STATIC ${lib-src})
target_compile_options(${PROJECT_NAME}lib PRIVATE "-Wno-old-style-cast" "-Wno-unused-macros" "-Wno-unknown-warning-option")

add_executable(${PROJECT_NAME}-local ${local-src})
target_link_libraries(${PROJECT_NAME}-local ${PROJECT_NAME}lib)
INSTALL(TARGETS ${PROJECT_NAME}-local DESTINATION bin)

add_executable(${PROJECT_NAME}-remote ${remote-src})
target_link_libraries(${PROJECT_NAME}-remote ${PROJECT_NAME}lib)
INSTALL(TARGETS ${PROJECT_NAME}-remote DESTINATION bin)
