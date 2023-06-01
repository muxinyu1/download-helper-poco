cmake_minimum_required(VERSION 3.0.0)
project(download-helper VERSION 0.1.0)

include(CTest)
enable_testing()

set(CMAKE_CXX_STANDARD 17)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(fmt REQUIRED)
find_package(Poco REQUIRED Foundation XML JSON Net Util NetSSL)

file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")

add_executable(download-helper ${HEADERS} main.cxx lib.cxx)

target_link_libraries(download-helper Poco::Foundation Poco::XML Poco::JSON Poco::Net Poco::Util Poco::NetSSL OpenSSL::SSL fmt::fmt)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
