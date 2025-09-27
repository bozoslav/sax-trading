cmake_minimum_required(VERSION 3.10)
project(exchange-backend)

set(CMAKE_CXX_STANDARD 17)

# Homebrew Boost hint (optional)
set(BOOST_ROOT /opt/homebrew/opt/boost CACHE PATH "Boost root")
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /opt/homebrew/opt/boost)

find_package(Boost REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQXX REQUIRED libpqxx)
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(
	${CMAKE_SOURCE_DIR}/include
	${Boost_INCLUDE_DIRS}
	${LIBPQXX_INCLUDE_DIRS}
	${PostgreSQL_INCLUDE_DIRS}
	${OPENSSL_INCLUDE_DIR}
)
link_directories(${LIBPQXX_LIBRARY_DIRS} ${PostgreSQL_LIBRARY_DIRS})

add_executable(exchange-backend main.cpp http_server.cpp)

target_link_libraries(exchange-backend
	PRIVATE
		${Boost_LIBRARIES}
		${LIBPQXX_LIBRARIES}
		${PostgreSQL_LIBRARIES}
		OpenSSL::SSL OpenSSL::Crypto
		pthread
)
