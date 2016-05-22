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

# Check for h-files
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdio.h HAVE_STDIO_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(errno.h HAVE_ERRNO_H)

# Check sizes of data types
check_type_size(int64_t INT64_T)
check_type_size("long long" LONG_LONG_INT)
check_type_size(int32_t INT32_T)
check_type_size("long" LONG_INT)
check_type_size("int" INT)
check_type_size("unsigned long" UNSIGNED_LONG_INT)
check_type_size("unsigned int" UNSIGNED_INT)
check_type_size("unsigned short" UNSIGNED_SHORT)
check_type_size(uint32_t UINT32_T)
check_type_size(uint16_t UINT16_T)
check_type_size(uint8_t UINT8_T)
check_type_size(ssize_t SSIZE_T)
check_type_size(size_t SIZE_T)

#
# Prepare build / define targets
#

# Copy the nljson.h file to the build dir
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/include/nljson.h
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/src)

set(NLJSON_LIB_SRC src/lib/nljson.c src/lib/nljson_encode.c)
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

if (CMAKE_COMPILER_IS_GNUCC)
	add_definitions(-Wall -Wextra -Wdeclaration-after-statement)
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

# Install pkg-config file
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/nljson.pc
        DESTINATION ${NLJSON_INSTALL_LIB_DIR}/pkgconfig COMPONENT dev)
