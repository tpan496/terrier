# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.16.3)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(fast_float-populate NONE)

include(ExternalProject)
ExternalProject_Add(fast_float-populate
                     "GIT_REPOSITORY" "https://github.com/lemire/fast_float.git" "GIT_TAG" "v1.0.0"
                    SOURCE_DIR          "/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/src/fast_float"
                    BINARY_DIR          "/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/build/fast_float"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)
