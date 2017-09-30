cmake_minimum_required(VERSION 3.6)

# Name of the project (will be the name of the plugin)
project(measurement-kit)

# Essential include files to build a node addon,
# you should add this line in every CMake.js based project.
include_directories(${CMAKE_JS_INC})

# Declare the location of the source files
file(
    GLOB
    SOURCE_FILES
    "src/*.cpp"
    "src/*/*.cpp"
    "src/*.hpp"
    "src/*/*.hpp"
)

# Enable C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Find MeasurementKit using our custom module
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
#set(MEASUREMENTKIT_ROOT "MAYBE tweak this")
# This is what does the magic
find_package(MeasurementKit REQUIRED)
include_directories(${MEASUREMENTKIT_INCLUDE_DIRS})

# This line will tell CMake that we're building a shared library
# from the above source files
# named after the project's name
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

# Gives our library file a .node extension without any "lib" prefix
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

message(STATUS "LIBRARIES ${MEASUREMENTKIT_LIBRARIES} | ${MEASUREMENTKIT_INCLUDE_DIRS}")
message(STATUS "OpenSSL Version ${OPENSSL_VERSION} | ${OPENSSL_LIBRARIES}")
# Link everything together
target_link_libraries(${PROJECT_NAME}
    ${CMAKE_JS_LIB}
    ${OPENSSL_LIBRARIES}
    ${MEASUREMENTKIT_LIBRARIES}
)
