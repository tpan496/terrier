# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash

# Utility rule file for xxhash-populate.

# Include the progress variables for this target.
include CMakeFiles/xxhash-populate.dir/progress.make

CMakeFiles/xxhash-populate: CMakeFiles/xxhash-populate-complete


CMakeFiles/xxhash-populate-complete: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-install
CMakeFiles/xxhash-populate-complete: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-mkdir
CMakeFiles/xxhash-populate-complete: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-download
CMakeFiles/xxhash-populate-complete: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-update
CMakeFiles/xxhash-populate-complete: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-patch
CMakeFiles/xxhash-populate-complete: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-configure
CMakeFiles/xxhash-populate-complete: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-build
CMakeFiles/xxhash-populate-complete: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-install
CMakeFiles/xxhash-populate-complete: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-test
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Completed 'xxhash-populate'"
	/usr/bin/cmake -E make_directory /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles
	/usr/bin/cmake -E touch /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles/xxhash-populate-complete
	/usr/bin/cmake -E touch /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-done

xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-install: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-build
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "No install step for 'xxhash-populate'"
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/build/xxHash && /usr/bin/cmake -E echo_append
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/build/xxHash && /usr/bin/cmake -E touch /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-install

xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-mkdir:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Creating directories for 'xxhash-populate'"
	/usr/bin/cmake -E make_directory /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/src/xxHash
	/usr/bin/cmake -E make_directory /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/build/xxHash
	/usr/bin/cmake -E make_directory /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix
	/usr/bin/cmake -E make_directory /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/tmp
	/usr/bin/cmake -E make_directory /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src/xxhash-populate-stamp
	/usr/bin/cmake -E make_directory /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src
	/usr/bin/cmake -E make_directory /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src/xxhash-populate-stamp
	/usr/bin/cmake -E touch /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-mkdir

xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-download: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-gitinfo.txt
xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-download: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-mkdir
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Performing download step (git clone) for 'xxhash-populate'"
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/src && /usr/bin/cmake -P /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/tmp/xxhash-populate-gitclone.cmake
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/src && /usr/bin/cmake -E touch /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-download

xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-update: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-download
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Performing update step for 'xxhash-populate'"
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/src/xxHash && /usr/bin/cmake -P /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/tmp/xxhash-populate-gitupdate.cmake

xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-patch: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-download
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "No patch step for 'xxhash-populate'"
	/usr/bin/cmake -E echo_append
	/usr/bin/cmake -E touch /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-patch

xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-configure: xxhash-populate-prefix/tmp/xxhash-populate-cfgcmd.txt
xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-configure: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-update
xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-configure: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-patch
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "No configure step for 'xxhash-populate'"
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/build/xxHash && /usr/bin/cmake -E echo_append
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/build/xxHash && /usr/bin/cmake -E touch /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-configure

xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-build: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-configure
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "No build step for 'xxhash-populate'"
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/build/xxHash && /usr/bin/cmake -E echo_append
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/build/xxHash && /usr/bin/cmake -E touch /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-build

xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-test: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-install
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "No test step for 'xxhash-populate'"
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/build/xxHash && /usr/bin/cmake -E echo_append
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/build/xxHash && /usr/bin/cmake -E touch /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-test

xxhash-populate: CMakeFiles/xxhash-populate
xxhash-populate: CMakeFiles/xxhash-populate-complete
xxhash-populate: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-install
xxhash-populate: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-mkdir
xxhash-populate: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-download
xxhash-populate: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-update
xxhash-populate: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-patch
xxhash-populate: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-configure
xxhash-populate: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-build
xxhash-populate: xxhash-populate-prefix/src/xxhash-populate-stamp/xxhash-populate-test
xxhash-populate: CMakeFiles/xxhash-populate.dir/build.make

.PHONY : xxhash-populate

# Rule to build all files generated by this target.
CMakeFiles/xxhash-populate.dir/build: xxhash-populate

.PHONY : CMakeFiles/xxhash-populate.dir/build

CMakeFiles/xxhash-populate.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/xxhash-populate.dir/cmake_clean.cmake
.PHONY : CMakeFiles/xxhash-populate.dir/clean

CMakeFiles/xxhash-populate.dir/depend:
	cd /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash /mnt/c/private/noisepage/cmake-build-debug-wsl/_deps/sub/xxHash/CMakeFiles/xxhash-populate.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/xxhash-populate.dir/depend

