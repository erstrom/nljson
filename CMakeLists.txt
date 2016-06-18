cmake_minimum_required(VERSION 2.8)
project(nljson C)

include(CheckIncludeFiles)
include(CheckTypeSize)
include(FindPkgConfig)

set(NLJSON_VERSION "0.1")
set(NLJSON_SOVERSION 0)
set(NLJSON_TOOLS_VERSION "0.1")

#
# nljson configuration
#
option(NLJSON_BUILD_SHARED_LIB "Build shared library." ON)
option(NLJSON_BUILD_ENCODER "Build encoder program." ON)
option(NLJSON_BUILD_DECODER "Build decoder program." ON)
option(NLJSON_USE_INT64 "Use 64 bit integer type for JSON integers." ON)
option(NLJSON_DEBUG "Add debug info to binaries." OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

#
# Dependency check
#
message("Using pkg-config: " ${PKG_CONFIG_EXECUTABLE})

# Check for libnl
pkg_check_modules(LIBNL REQUIRED libnl-3.0>=3.0)

link_directories(${LIBNL_LIBRARY_DIRS})
include_directories(${LIBNL_INCLUDE_DIRS})

# Check for jansson
pkg_check_modules(JANSSON REQUIRED jansson>=2.0)

link_directories(${JANSSON_LIBRARY_DIRS})
include_directories(${JANSSON_INCLUDE_DIRS})

if (NOT LIBNL_FOUND OR NOT JANSSON_FOUND)
	message(FATAL_ERROR "Missing dependecies")
endif()

# Check for h-files
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdbool.h HAVE_STDBOOL_H)
check_include_files(stdio.h HAVE_STDIO_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(errno.h HAVE_ERRNO_H)
if (NLJSON_BUILD_ENCODER OR NLJSON_BUILD_DECODER)
	check_include_files(getopt.h HAVE_GETOPT_H)
endif()

# Check sizes of data types
check_type_size(int64_t INT64_T)
check_type_size("long long" LONG_LONG)
if (NOT HAVE_INT64_T AND (NOT HAVE_LONG_LONG OR NOT (${LONG_LONG} EQUAL 8) ))
	message(FATAL_ERROR "Missing 64 bit integer type!")
endif()
check_type_size(int32_t INT32_T)
check_type_size(long LONG)
if (NOT HAVE_INT32_T AND (NOT HAVE_LONG OR NOT (${LONG} EQUAL 4) ))
	message(FATAL_ERROR "Missing 32 bit integer type!")
endif()
check_type_size(uint32_t UINT32_T)
check_type_size("unsigned long" UNSIGNED_LONG)
if (NOT HAVE_UINT32_T AND (NOT HAVE_UNSIGNED_LONG OR NOT (${UNSIGNED_LONG} EQUAL 4) ))
	message(FATAL_ERROR "Missing unsigned 32 bit integer type!")
endif()
check_type_size(uint16_t UINT16_T)
check_type_size("unsigned short" UNSIGNED_SHORT)
if (NOT HAVE_UINT16_T AND (NOT HAVE_UNSIGNED_SHORT OR NOT (${UNSIGNED_SHORT} EQUAL 2) ))
	message(FATAL_ERROR "Missing unsigned 16 bit integer type!")
endif()
check_type_size(uint8_t UINT8_T)
check_type_size("unsigned char" UNSIGNED_CHAR)
if (NOT HAVE_UINT8_T AND (NOT HAVE_UNSIGNED_CHAR OR NOT (${UNSIGNED_CHAR} EQUAL 1) ))
	message(FATAL_ERROR "Missing unsigned 8 bit integer type!")
endif()
check_type_size(ssize_t SSIZE_T)
if (NOT HAVE_SSIZE_T)
	message(FATAL_ERROR "Missing ssize_t!")
endif()
check_type_size(size_t SIZE_T)
if (NOT HAVE_SIZE_T)
	message(FATAL_ERROR "Missing size_t!")
endif()

#
# Prepare build / define targets
#

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/lib/nljson_config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/nljson_config.h)

# Copy the nljson.h file to the build dir
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/include/nljson.h
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/src)

set(NLJSON_LIB_SRC src/lib/nljson.c src/lib/nljson_encode.c src/lib/nljson_decode.c)
set(NLJSON_ENCODER_SRC src/tools/nljson-encoder.c)
set(NLJSON_DECODER_SRC src/tools/nljson-decoder.c)
set(NLJSON_HDR_PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include/nljson.h)

if(NLJSON_BUILD_SHARED_LIB)
	add_library(nljson SHARED
	            ${NLJSON_LIB_SRC}
	            ${NLJSON_HDR_PUBLIC}
	            src/lib/nljson.def)

	set_target_properties(nljson PROPERTIES
	                      VERSION ${NLJSON_VERSION}
	                      SOVERSION ${NLJSON_SOVERSION})
else()
	add_library(nljson
	            ${NLJSON_LIB_SRC}
	            ${NLJSON_HDR_PUBLIC})
endif()

target_link_libraries(nljson ${JANSSON_LIBRARIES} ${LIBNL_LIBRARIES})

if (NLJSON_BUILD_ENCODER)
	add_executable(nljson-encoder
	               ${NLJSON_ENCODER_SRC}
	               ${NLJSON_HDR_PUBLIC})
	target_link_libraries(nljson-encoder nljson)
endif()

if (NLJSON_BUILD_DECODER)
	add_executable(nljson-decoder
	               ${NLJSON_DECODER_SRC}
	               ${NLJSON_HDR_PUBLIC})
	target_link_libraries(nljson-decoder nljson)
endif()

if (CMAKE_COMPILER_IS_GNUCC)
	add_definitions(-Wall -Wextra -Wdeclaration-after-statement)
endif()

if (NLJSON_DEBUG)
	add_definitions(-g -O0)
endif()

# Allow the user to override installation directories.
set(NLJSON_INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(NLJSON_INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(NLJSON_INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")

# Create pkg-config file.
set(prefix ${CMAKE_INSTALL_PREFIX})
set(INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/${NLJSON_INSTALL_LIB_DIR})
set(INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/${NLJSON_INSTALL_INCLUDE_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/nljson.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/nljson.pc @ONLY)

# Define the public headers.
set_target_properties(nljson PROPERTIES PUBLIC_HEADER "${NLJSON_HDR_PUBLIC}")

#
# Install
#

# Install artifacts
install(TARGETS nljson
        LIBRARY DESTINATION "${NLJSON_INSTALL_LIB_DIR}" COMPONENT lib
        ARCHIVE DESTINATION "${NLJSON_INSTALL_LIB_DIR}" COMPONENT lib
        RUNTIME DESTINATION "${NLJSON_INSTALL_BIN_DIR}" COMPONENT bin
        PUBLIC_HEADER DESTINATION "${NLJSON_INSTALL_INCLUDE_DIR}" COMPONENT dev)

if (NLJSON_BUILD_ENCODER)
	install(TARGETS nljson-encoder
	        RUNTIME DESTINATION "${NLJSON_INSTALL_BIN_DIR}" COMPONENT bin)
endif()

if (NLJSON_BUILD_DECODER)
	install(TARGETS nljson-decoder
	        RUNTIME DESTINATION "${NLJSON_INSTALL_BIN_DIR}" COMPONENT bin)
endif()

# Install pkg-config file
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/nljson.pc
        DESTINATION ${NLJSON_INSTALL_LIB_DIR}/pkgconfig COMPONENT dev)

