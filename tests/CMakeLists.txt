
cmake_minimum_required(VERSION 2.6)
 
# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})



file(GLOB ../include/*.cpp include)
file(GLOB ../../fledge-south-hnz/*.cpp fledge-south-hnz) 
file(GLOB ../../fledge/*.cpp fledge) 
file(GLOB ../../fledge-south-hnz/*.h fledge-south-hnz)
file(GLOB ../../fledge/*.h fledge)
file(GLOB ../include/*.h include)







 
# Link runTests with what we want to test and the GTest and pthread library
add_executable(executeTests test_PrepareParameters.cpp)
target_link_libraries(executeTests ${GTEST_LIBRARIES} pthread)






