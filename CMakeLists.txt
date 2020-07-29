cmake_minimum_required(VERSION 3.8)
project(Dedispersion VERSION 5.0)
include(GNUInstallDirs)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native -mtune=native")
set(TARGET_LINK_LIBRARIES dedispersion isa_utils isa_opencl astrodata OpenCL)
if($ENV{LOFAR})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_HDF5")
  set(TARGET_LINK_LIBRARIES ${TARGET_LINK_LIBRARIES} hdf5 hdf5_cpp z)
endif()
if($ENV{PSRDADA})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_PSRDADA")
  set(TARGET_LINK_LIBRARIES ${TARGET_LINK_LIBRARIES} psrdada cudart)
endif()

set(DEDISPERSION_HEADER
  include/configuration.hpp
  include/Dedispersion.hpp
  include/Shifts.hpp
)

# libdedispersion
add_library(dedispersion SHARED
  src/Dedispersion.cpp
  src/Shifts.cpp
)
set_target_properties(dedispersion PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
  PUBLIC_HEADER "include/Dedispersion.hpp;include/Shifts.hpp"
)
target_include_directories(dedispersion PRIVATE include)

# DedispersionTesting
add_executable(DedispersionTesting
  src/DedispersionTest.cpp
  ${DEDISPERSION_HEADER}
)
target_include_directories(DedispersionTesting PRIVATE include)
target_link_libraries(DedispersionTesting PRIVATE ${TARGET_LINK_LIBRARIES})

# DedispersionTuning
add_executable(DedispersionTuning
  src/DedispersionTuning.cpp
  ${DEDISPERSION_HEADER}
)
target_include_directories(DedispersionTuning PRIVATE include)
target_link_libraries(DedispersionTuning PRIVATE ${TARGET_LINK_LIBRARIES})

install(TARGETS dedispersion DedispersionTesting DedispersionTuning
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
