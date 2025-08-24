cmake_minimum_required(VERSION 3.10)

project(exchange-backend)


set(CMAKE_CXX_STANDARD 17)

set(BOOST_ROOT /opt/homebrew/opt/boost)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /opt/homebrew/opt/boost)

find_package(Boost REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQXX REQUIRED libpqxx)

include_directories(${Boost_INCLUDE_DIRS} ${LIBPQXX_INCLUDE_DIRS})
link_directories(${LIBPQXX_LIBRARY_DIRS})

add_executable(exchange-backend main.cpp)

target_link_libraries(exchange-backend ${Boost_LIBRARIES} ${LIBPQXX_LIBRARIES} pq pqxx pthread)
