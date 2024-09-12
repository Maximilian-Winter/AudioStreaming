cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 20)

project(AudioStreaming)

# Set global output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


# Define ASIO_STANDALONE to avoid needing Boost
add_definitions(-DASIO_STANDALONE)

# Set Asio include directory
set(ASIO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/asio-1-28-0/asio/include/)
# Set PortAudio directory
set(PORTAUDIO_DIR ${CMAKE_SOURCE_DIR}/portaudio)


# Add PortAudio subdirectory
add_subdirectory(${PORTAUDIO_DIR})
# Check if Asio directory exists
if(NOT EXISTS ${ASIO_INCLUDE_DIR})
    message(FATAL_ERROR "Asio include directory not found: ${ASIO_INCLUDE_DIR}")
endif()

# Add the include directory for Asio
include_directories(${ASIO_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${nlohmann_json_INCLUDE_DIRS})

# Print Asio include path for debugging
message(STATUS "Asio include path: ${ASIO_INCLUDE_DIR}")

# Function to add common settings to targets
function(add_common_settings target)
    target_include_directories(${target} PRIVATE
            ${ASIO_INCLUDE_DIR}
            ${PORTAUDIO_DIR}/include
    )
endfunction()

add_subdirectory(audio)

# Print include directories for debugging
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "Include dir: '${dir}'")
endforeach()
