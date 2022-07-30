

cmake_minimum_required(VERSION 2.8)

project(RunTests)

# Supported options:
# -DFLEDGE_INCLUDE
# -DFLEDGE_LIB
# -DFLEDGE_SRC
# -DFLEDGE_INSTALL
#
# If no -D options are given and FLEDGE_ROOT environment variable is set
# then Fledge libraries and header files are pulled from FLEDGE_ROOT path.

set(CMAKE_CXX_FLAGS "-std=c++11 -O3")


include_directories(${CMAKE_BINARY_DIR})

# Set plugin type (south, north, filter)
set(PLUGIN_TYPE "north")

# Add here all needed Fledge libraries as list
set(NEEDED_FLEDGE_LIBS common-lib services-common-lib)

set(BOOST_COMPONENTS system thread)

find_package(Boost 1.53.0 COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

# Find source files
file(GLOB SOURCES ../*.cpp)
file(GLOB unittests "*.cpp")


# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Add ../include
include_directories(../include)
include_directories(/usr/local/include/lib60870)
# Add Fledge include dir(s)
include_directories(${FLEDGE_INCLUDE_DIRS})

# Add Fledge lib path
link_directories(${FLEDGE_LIB_DIRS})

# Link runTests with what we want to test and the GTest and pthread library

set(FLEDGE_INSTALL "" CACHE INTERNAL "")



# Link runTests with what we want to test and the GTest and pthread library
add_executable(executeTests test_PrepareParameters.cpp)
target_link_libraries(executeTests ${GTEST_LIBRARIES} pthread)



target_link_libraries(${PROJECT_NAME} ${NEEDED_FLEDGE_LIBS})
target_link_libraries(${PROJECT_NAME}  ${Boost_LIBRARIES})




