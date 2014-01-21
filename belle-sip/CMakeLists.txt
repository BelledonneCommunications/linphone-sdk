cmake_minimum_required(VERSION 2.6)
project(BELLESIP C)

include_directories(
	include/
	${CMAKE_CURRENT_BINARY_DIR}/src/
	src/
	${CMAKE_INSTALL_PREFIX}/include
)

add_subdirectory(include)
add_subdirectory(src)
