cmake_minimum_required(VERSION 3.10)
project(DE430Parser VERSION 1.0)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include directories for header files
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Define source files
set(SOURCES
        src/de430_parser.c
        src/cJSON.c
        src/json.c
        src/binary.c
        src/csv.c
        # Add any other source files here
)

# Create library target
add_library(de430docker ${SOURCES})

# Create executable that uses the library
add_executable(main src/main.c)

# Link the library with the executable
target_link_libraries(main de430docker)