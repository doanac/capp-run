# Copyright (c) 2021 Foundries.io
#
# SPDX-License-Identifier: MIT

cmake_minimum_required (VERSION 3.5)

project(capp-run)

add_definitions('-g')
add_definitions('-Wall')

find_package(PkgConfig REQUIRED)

set(CMAKE_CXX_STANDARD 14)

add_executable(capp-run src/main.cpp)
target_include_directories(capp-run PRIVATE ${CMAKE_SOURCE_DIR}/third-party)

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
