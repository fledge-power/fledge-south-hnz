cmake_minimum_required(VERSION 2.6)
 
# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

#include_directories(../include)
#include_directories(../../fledge-south-hnz)
#include_directories(../../libhnz/src/hnz/)
#include_directories(../../libhnz/src/hal/inc)
#include_directories(../../libhnz/src/hal/srv)
#include_directories(../../libhnz)
#include_directories(/usr/src/gtest)

#link_directories(${FLEDGE_LIB_DIRS})
#include_directories(${FLEDGE_INCLUDE_DIRS})
#include_directories(${GTEST_INCLUDE_DIRS})
#include_directories(SYSTEM ${Boost_INCLUDE_DIR})
#include_directories(${CMAKE_BINARY_DIR})

#include_directories(${PROJECT_SOURCE_DIR}../../fledge)
#include_directories(${PROJECT_SOURCE_DIR}../../libhnz)




file(GLOB ../../libhnz/*.cpp SRC) 


# Link runTests with what we want to test and the GTest and pthread library

#add_executable(executeTests test_PrepareParameters.cpp)
#target_link_libraries(executeTests ${GTEST_LIBRARIES} pthread)

add_executable(
        my_exe
        ${source_files}
)


