# Copyright (c) 2021 Foundries.io
#
# SPDX-License-Identifier: MIT

cmake_minimum_required (VERSION 3.5)

option(DOCKER_ARCH "Set the docker architecture supported")
if(DOCKER_ARCH)
	add_definitions(-DDOCKER_ARCH="${DOCKER_ARCH}")
	message(STATUS "DOCKER_ARCH=${DOCKER_ARCH}")
else()
	message(FATAL_ERROR "DOCKER_ARCH required")
endif(DOCKER_ARCH)

project(capp-run)

add_definitions('-g')
add_definitions('-Wall')

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS filesystem REQUIRED)
find_package(PkgConfig REQUIRED)

set(CMAKE_CXX_STANDARD 14)

add_executable(capp-run src/main.cpp src/capp.cpp src/context.cpp src/net.cpp src/oci-hooks.cpp src/project.cpp src/utils.cpp)
target_include_directories(capp-run PRIVATE ${CMAKE_SOURCE_DIR}/third-party ${CMAKE_SOURCE_DIR}/third-party/yaml-cpp/include)
target_link_libraries(capp-run -lpthread ${Boost_LIBRARIES} yaml-cpp)

install(TARGETS capp-run RUNTIME DESTINATION bin)

add_custom_target(check-format
	${CMAKE_SOURCE_DIR}/tools/run-clang-format --clang-format-executable=clang-format-10 ${CMAKE_SOURCE_DIR}/src/*
)
add_custom_target(clang-format
	${CMAKE_SOURCE_DIR}/tools/run-clang-format -i --clang-format-executable=clang-format-10 ${CMAKE_SOURCE_DIR}/src/*
)
add_custom_target(clang-tidy
	${CMAKE_SOURCE_DIR}/tools/run-clang-tidy ${CMAKE_SOURCE_DIR}/src
)

add_subdirectory(third-party/yaml-cpp)
