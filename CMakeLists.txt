cmake_minimum_required(VERSION 3.10)
project(Dedispersion VERSION 4.1)
include(GNUInstallDirs)

add_library(libdedispersion SHARED
  src/Dedispersion.cpp
  src/Shifts.cpp
)
set_target_properties(libdedispersion PROPERTIES 
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
  PUBLIC_HEADER "include/Dedispersion.hpp;include/Shifts.hpp"
)
target_include_directories(libdedispersion PRIVATE include)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g0 -march=native -mtune=native")

add_executable(DedispersionTesting
  src/DedispersionTest.cpp
)
target_include_directories(DedispersionTesting PRIVATE include)
target_link_libraries(DedispersionTesting dedispersion isa_utils isa_opencl astrodata)

add_executable(DedispersionTuning
  src/DedispersionTuning.cpp
)
target_include_directories(DedispersionTuning PRIVATE include)
target_link_libraries(DedispersionTuning dedispersion isa_utils isa_opencl astrodata)

install(TARGETS libdedispersion DedispersionTesting DedispersionTuning
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
