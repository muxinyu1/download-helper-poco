cmake_minimum_required(VERSION 3.0.0)
project(download)

set(CMAKE_CXX_STANDARD 20)


find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(fmt REQUIRED)
find_package(Poco REQUIRED Foundation XML JSON Net Util NetSSL)
find_package(Boost REQUIRED COMPONENTS serialization)
find_package(SQLite3 REQUIRED)

file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")

add_executable(${PROJECT_NAME} ${HEADERS} main.cxx lib.cxx)

target_link_libraries(${PROJECT_NAME} Poco::Foundation Poco::XML Poco::JSON Poco::Net Poco::Util Poco::NetSSL OpenSSL::SSL fmt::fmt Boost::serialization SQLite::SQLite3)

install(TARGETS ${PROJECT_NAME} DESTINATION /usr/local/bin)