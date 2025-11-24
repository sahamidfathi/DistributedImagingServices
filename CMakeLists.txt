cmake_minimum_required(VERSION 3.16)
project(DistributedImaging CXX)

# Require C++17 for std::filesystem
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------ Packages ------------------

# OpenCV: core, imgcodecs (for imread/imencode), imgproc, features2d (for SIFT)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc features2d)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found!")
endif()
message(STATUS "Found OpenCV: ${OpenCV_LIBS}")

# ZeroMQ via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
if(NOT ZMQ_FOUND)
    message(FATAL_ERROR "ZeroMQ not found!")
endif()
message(STATUS "Found ZeroMQ: ${ZMQ_LIBRARIES}")

# SQLite3 via pkg-config
pkg_check_modules(SQLite3 REQUIRED sqlite3)
if(NOT SQLite3_FOUND)
    message(FATAL_ERROR "SQLite3 not found!")
endif()
message(STATUS "Found SQLite3: ${SQLite3_LIBRARIES}")

# System includes for ZMQ and SQLite
include_directories(SYSTEM ${ZMQ_INCLUDE_DIRS})
include_directories(SYSTEM ${SQLite3_INCLUDE_DIRS})

# ------------------ Common library ------------------

# This library holds the shared serialization logic (and can expose common headers)
add_library(common
    src/common/Serialization.cpp
)

# Public headers now live in include/
target_include_directories(common PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(common
    ${OpenCV_LIBS}
)

# ------------------ Applications ------------------

# Application 1: Image Generator
add_executable(image_generator
    src/generator/main.cpp
)

target_include_directories(image_generator PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(image_generator
    common            # not strictly required, but fine to link
    ${OpenCV_LIBS}
    ${ZMQ_LIBRARIES}
)

# Application 2: Feature Extractor
add_executable(feature_extractor
    src/extractor/main.cpp
)

target_include_directories(feature_extractor PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(feature_extractor
    common            # serialization + shared headers
    ${OpenCV_LIBS}
    ${ZMQ_LIBRARIES}
)

# Application 3: Data Logger
add_executable(data_logger
    src/logger/main.cpp
)

target_include_directories(data_logger PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(data_logger
    common
    ${ZMQ_LIBRARIES}
    ${SQLite3_LIBRARIES}
)

# Install targets (optional)
install(TARGETS image_generator feature_extractor data_logger DESTINATION bin)

