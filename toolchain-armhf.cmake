set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(TOOLCHAIN_PREFIX arm-linux-gnueabihf)

set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armhf")
set (TEST OFF)
# needed for using on different linux versions (e.g. compiling with ubuntu 19.10 for raspian, or with future versions of ubuntu)
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")

# needed to support the raspberry pi zero
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv6 -mfpu=vfp -mfloat-abi=hard")
#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv6 -mfpu=vfp -mfloat-abi=hard")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -marm -mfpu=vfp -mfloat-abi=hard")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -marm -mfpu=vfp -mfloat-abi=hard")

# cross compilers to use for C and C++
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc${COMPILER_SUFFIX})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++${COMPILER_SUFFIX})

# target environment on the build host system
#   set 1st to dir with the cross compiler's C/C++ headers/libs
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
