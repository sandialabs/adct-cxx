cmake_minimum_required(VERSION 3.8)
project (adc_cxx
         LANGUAGES CXX C
         VERSION 0.1.0)

set(ADC_BUILDER "cmake")
set(BLT_CXX_STD "c++17" CACHE STRING "Version of C++ standard")

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR lib)
endif()

set(BLT_EXPORT_THIRDPARTY ON CACHE BOOL "")

if (DEFINED BLT_SOURCE_DIR)
    # Support having a shared BLT outside of the repository if given a BLT_SOURCE_DIR
    if (NOT EXISTS ${BLT_SOURCE_DIR}/SetupBLT.cmake)
        message(FATAL_ERROR "Given BLT_SOURCE_DIR does not contain SetupBLT.cmake")
    endif()
else()
    # Use internal BLT if no BLT_SOURCE_DIR is given
    set(BLT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/blt" CACHE PATH "")
    if (NOT EXISTS ${BLT_SOURCE_DIR}/SetupBLT.cmake)
        message(FATAL_ERROR
            "The BLT git submodule is not present. "
            "Either run the following two commands in your git repository: \n"
            "    git submodule init\n"
            "    git submodule update\n"
            "Or add -DBLT_SOURCE_DIR=/path/to/blt to your CMake command." )
    endif()
endif()

include(${BLT_SOURCE_DIR}/SetupBLT.cmake)

# find_package(adiak PATHS ENV ADIAK_PREFIX)
find_package(adiak)

# pull in libuuid, assumed in /usr
blt_import_library( NAME                     uuid
                    LINK_FLAGS               -luuid)
                    
## Set the compiler flags; preferably on cmake line if needed.
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -g -O0 -Wall")

if (ENABLE_DOC)
  add_subdirectory(doc)
endif()

include(CMakePackageConfigHelpers)

# Configure adc_cxx-config-version.cmake
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/adc_cxx-config-version.cmake"
  VERSION ${LIBTOOL_INTERFACE}.${LIBTOOL_REVISION}.${LIBTOOL_AGE}
  COMPATIBILITY SameMajorVersion)

set(adc_cxx_INSTALL_INCLUDE_DIR include/)

# Configure adiak-config.cmake
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/adc_cxx-config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/adc_cxx"
  PATH_VARS adc_cxx_INSTALL_INCLUDE_DIR)

# Enable the use of Adiak
#option(USE_ADIAK_BA "Enable Adiak support" ON)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/adc_cxx-config.cmake" "${CMAKE_CURRENT_BINARY_DIR}/adc_cxx-config-version.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/adc_cxx")


# Set the linker flags
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lstdc++fs")

# Set the include directories
include_directories(${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${BOOST_ROOT}/include)

# Set the library directories
link_directories(${BOOST_ROOT}/lib)

# Define the source files
set(adc_cxx_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/adc/impl/adc.cpp
)

# Define the generated headers
set(GENHEADERS
	${CMAKE_BINARY_DIR}/adc/adc_config.h
)

set(adc_cxx_public_headers
	adc/adc.hpp
	adc/types.hpp
	adc/factory.hpp
	adc/builder/impl/builder.hpp
	adc/builder/builder.hpp
	adc/publisher/multi_publisher.hpp
	adc/publisher/publisher.hpp
	${CMAKE_BINARY_DIR}/adc/adc_config.h)

set(adc_cxx_top_headers
	adc/adc.hpp
	adc/types.hpp
	adc/factory.hpp
	${CMAKE_BINARY_DIR}/adc/adc_config.h)

set(adc_cxx_bld_headers
	adc/builder/builder.hpp)

set(adc_cxx_pub_headers
	adc/publisher/multi_publisher.hpp
	adc/publisher/publisher.hpp)


if (MPI_FOUND)
	add_definitions("-DUSE_MPI")
	set(ADC_HAVE_MPI 1)
	list(APPEND adc_cxx_dependencies
		mpi)
	list(APPEND adc_cxx_export_targets
		mpi)
endif()

if (ADIAK_FOUND)
	add_definitions("-DUSE_ADIAK")
	set(ADC_HAVE_ADIAK 1)
	list(APPEND adc_cxx_dependencies
		adiak)
	list(APPEND adc_cxx_export_targets
		adiak)
endif()

list(APPEND adc_cxx_dependencies
	uuid)

# Generate the adc_config.h header
configure_file(adc/adc_config.h.in adc/adc_config.h)
add_custom_target(headers ALL DEPENDS ${GENHEADERS})

# try out visibility stuff
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)


# Define the library target
blt_add_library(NAME adc_cxx
		HEADERS    ${adc_cxx_public_headers}
		SOURCES    ${adc_cxx_sources}
		DEPENDS_ON ${adc_cxx_dependencies}
		SHARED)

include(GenerateExportHeader)
generate_export_header(adc_cxx)

target_include_directories(adc_cxx
  PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/adc>"
  "$<INSTALL_INTERFACE:include>")

# Set the library properties
set_target_properties(adc_cxx PROPERTIES
  SOVERSION 1
  VERSION 0.1
)

install(TARGETS
	adc_cxx ${adc_cxx_export_targets}
	EXPORT adc_cxx-targets
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(EXPORT adc_cxx-targets
	NAMESPACE adc::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/adc_cxx)



install(FILES
        ${adc_cxx_top_headers}
        DESTINATION include/adc)

install(FILES
        ${adc_cxx_bld_headers}
        DESTINATION include/adc/builder)

install(FILES
        ${adc_cxx_pub_headers}
        DESTINATION include/adc/publisher)

# Define the executable targets
blt_add_executable(NAME demo.builder
	SOURCES examples/demoBuilder.cpp
	DEPENDS_ON adc_cxx)

add_executable(mpi.demo.builder	examples/mpiDemoBuilder.cpp)
add_executable(mpi.simple.demo examples/mpiSimpleDemo.cpp)
add_executable(test.builder examples/testBuilder.cpp)

install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/demo.builder  ${CMAKE_BINARY_DIR}/bin/mpi.demo.builder  ${CMAKE_BINARY_DIR}/bin/mpi.simple.demo  ${CMAKE_BINARY_DIR}/bin/test.builder
	TYPE BIN )

# Set the executable properties
set_target_properties(demo.builder PROPERTIES
  LINK_FLAGS "-lstdc++fs"
)
set_target_properties(mpi.demo.builder PROPERTIES
  LINK_FLAGS "-lstdc++fs"
)
set_target_properties(mpi.simple.demo PROPERTIES
  LINK_FLAGS "-lstdc++fs"
)
set_target_properties(test.builder PROPERTIES
  LINK_FLAGS "-lstdc++fs"
)

# Link the executables against the library
#target_link_libraries(demo.builder adc_cxx)
target_link_libraries(mpi.demo.builder adc_cxx)
target_link_libraries(mpi.simple.demo adc_cxx)
target_link_libraries(test.builder adc_cxx)

