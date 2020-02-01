cmake_minimum_required(VERSION 3.9)
set(CMAKE_CXX_STANDARD 11)
project(lambda-dlr-tf LANGUAGES CXX)


find_package(aws-lambda-runtime REQUIRED)
find_package(AWSSDK COMPONENTS s3)
add_executable(${PROJECT_NAME} "main.cpp")

target_compile_options(${PROJECT_NAME} PRIVATE
      "-march=ivybridge"
      "-Wall"
      "-Wextra"
      "-Werror")

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

add_library(dlr SHARED IMPORTED)
add_library(tensorflow SHARED IMPORTED)
add_library(tensorflow_framework SHARED IMPORTED)
set_target_properties(dlr PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libdlr.so)
set_target_properties(tensorflow PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libtensorflow.so.1)
set_target_properties(tensorflow_framework PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libtensorflow_framework.so.1)

target_link_libraries(${PROJECT_NAME} PUBLIC AWS::aws-lambda-runtime ${AWSSDK_LINK_LIBRARIES} dlr tensorflow_framework tensorflow)

