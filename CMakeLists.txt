cmake_minimum_required(VERSION 3.10) # 3.14 handles PIC differently
project (clippy C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

OPTION(ALL_WARNINGS "Enable all possible warnings" ON)

if (ALL_WARNINGS)
	# enable all warnings and disable non relevant stuff
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything -Wreorder -Woverloaded-virtual -Wcast-align -Wdeprecated")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-disabled-macro-expansion") # avoid warnings in lua defines such as lua_replace
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-missing-prototypes -Wno-weak-vtables -Wno-zero-length-array -Wno-gnu-anonymous-struct -Wno-reserved-id-macro -Wno-nested-anon-types -Wno-missing-variable-declarations")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-padded")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-exit-time-destructors -Wno-global-constructors")
		# possible fix in the future?
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-thread-safety-analysis -Wno-thread-safety-negative")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-documentation -Wno-documentation-unknown-command")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-over-aligned") # getting weird errors that the default allocator guarantees 4 bytes, and 8 bytes is needed if std::function's are used
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option -Wno-zero-as-null-pointer-constant") # lots of false positives
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-extra-semi-stmt") # lots of issues with checkAssert(..); (warning there is an additional ; at the end)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-ctad-maybe-unsupported") # disable errors about missing angle brackets
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c99-extensions") # disable warnings about designated initializers are a C99 extensions (as it is allowed in C++20) (needed for macOS Catalina)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-template") # needed for Catch2 TEMPLATE_LIST_TEST_CASE
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-alloca") # for now disabled, sqlite3 integration (only place alloca is used) is possibly removed anyway
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-undefined-func-template") # goes wrong with Prometheus imports (goes wrong on compile time)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas -Wno-undef")
	elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Woverloaded-virtual") # -Weffc++
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
		#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsuggest-final-methods -Wsuggest-final-types")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable") # GCC 7.3 complains wrongfully about "error: unused variable '_'" (code: "for (auto& [_, subscriptionInfo] : subscriptions) {")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-array-bounds") # false positive in memcpy of NumberToStringObject (see https://bitpowder.com:2443/bitpowder/indigo/-/jobs/47193)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-redundant-move") # conflicts with clang 7.0; which requires the moves	
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overflow") # too many false positives in simplestring.h
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=missing-field-initializers") # needed for HTTPRequestOptions{}
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-mismatched-new-delete") # needed for debug.h InstanceDebug
	endif()
	if (NOT MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	endif()
endif (ALL_WARNINGS)

set(src
  lib.cpp
  simple-raw.cpp
  daemon.cpp
  remote.cpp)

add_executable(${PROJECT_NAME} ${src})
target_compile_options(${PROJECT_NAME} PRIVATE "-Wno-old-style-cast" "-Wno-unused-macros" "-Wno-unknown-warning-option")
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)
if (NOT CMAKE_SYSTEM_NAME MATCHES "Apple")
  INSTALL(FILES ${PROJECT_NAME}.conf DESTINATION /etc/ssh/sshd_config.d)
else()
  INSTALL(FILES ${PROJECT_NAME}.conf DESTINATION etc/ssh/sshd_config.d)
endif()
