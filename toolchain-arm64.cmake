set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(TOOLCHAIN_PREFIX aarch64-linux-gnu)

set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
set (TEST OFF)
# needed for using on different linux versions (e.g. compiling with ubuntu 19.10 for raspian, or with future versions of ubuntu)
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")

# cross compilers to use for C and C++
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc${COMPILER_SUFFIX})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++${COMPILER_SUFFIX})

# target environment on the build host system
#   set 1st to dir with the cross compiler's C/C++ headers/libs
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
