# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

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
CMAKE_SOURCE_DIR = /home/tysonv7/srsUE

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/tysonv7/srsUE/build

# Include any dependencies generated for this target.
include ue/src/CMakeFiles/ue.dir/depend.make

# Include the progress variables for this target.
include ue/src/CMakeFiles/ue.dir/progress.make

# Include the compile flags for this target's objects.
include ue/src/CMakeFiles/ue.dir/flags.make

ue/src/CMakeFiles/ue.dir/main.cc.o: ue/src/CMakeFiles/ue.dir/flags.make
ue/src/CMakeFiles/ue.dir/main.cc.o: ../ue/src/main.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/tysonv7/srsUE/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object ue/src/CMakeFiles/ue.dir/main.cc.o"
	cd /home/tysonv7/srsUE/build/ue/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/ue.dir/main.cc.o -c /home/tysonv7/srsUE/ue/src/main.cc

ue/src/CMakeFiles/ue.dir/main.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/ue.dir/main.cc.i"
	cd /home/tysonv7/srsUE/build/ue/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/tysonv7/srsUE/ue/src/main.cc > CMakeFiles/ue.dir/main.cc.i

ue/src/CMakeFiles/ue.dir/main.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/ue.dir/main.cc.s"
	cd /home/tysonv7/srsUE/build/ue/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/tysonv7/srsUE/ue/src/main.cc -o CMakeFiles/ue.dir/main.cc.s

ue/src/CMakeFiles/ue.dir/main.cc.o.requires:
.PHONY : ue/src/CMakeFiles/ue.dir/main.cc.o.requires

ue/src/CMakeFiles/ue.dir/main.cc.o.provides: ue/src/CMakeFiles/ue.dir/main.cc.o.requires
	$(MAKE) -f ue/src/CMakeFiles/ue.dir/build.make ue/src/CMakeFiles/ue.dir/main.cc.o.provides.build
.PHONY : ue/src/CMakeFiles/ue.dir/main.cc.o.provides

ue/src/CMakeFiles/ue.dir/main.cc.o.provides.build: ue/src/CMakeFiles/ue.dir/main.cc.o

ue/src/CMakeFiles/ue.dir/ue.cc.o: ue/src/CMakeFiles/ue.dir/flags.make
ue/src/CMakeFiles/ue.dir/ue.cc.o: ../ue/src/ue.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/tysonv7/srsUE/build/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object ue/src/CMakeFiles/ue.dir/ue.cc.o"
	cd /home/tysonv7/srsUE/build/ue/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/ue.dir/ue.cc.o -c /home/tysonv7/srsUE/ue/src/ue.cc

ue/src/CMakeFiles/ue.dir/ue.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/ue.dir/ue.cc.i"
	cd /home/tysonv7/srsUE/build/ue/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/tysonv7/srsUE/ue/src/ue.cc > CMakeFiles/ue.dir/ue.cc.i

ue/src/CMakeFiles/ue.dir/ue.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/ue.dir/ue.cc.s"
	cd /home/tysonv7/srsUE/build/ue/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/tysonv7/srsUE/ue/src/ue.cc -o CMakeFiles/ue.dir/ue.cc.s

ue/src/CMakeFiles/ue.dir/ue.cc.o.requires:
.PHONY : ue/src/CMakeFiles/ue.dir/ue.cc.o.requires

ue/src/CMakeFiles/ue.dir/ue.cc.o.provides: ue/src/CMakeFiles/ue.dir/ue.cc.o.requires
	$(MAKE) -f ue/src/CMakeFiles/ue.dir/build.make ue/src/CMakeFiles/ue.dir/ue.cc.o.provides.build
.PHONY : ue/src/CMakeFiles/ue.dir/ue.cc.o.provides

ue/src/CMakeFiles/ue.dir/ue.cc.o.provides.build: ue/src/CMakeFiles/ue.dir/ue.cc.o

ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o: ue/src/CMakeFiles/ue.dir/flags.make
ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o: ../ue/src/metrics_stdout.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/tysonv7/srsUE/build/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o"
	cd /home/tysonv7/srsUE/build/ue/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/ue.dir/metrics_stdout.cc.o -c /home/tysonv7/srsUE/ue/src/metrics_stdout.cc

ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/ue.dir/metrics_stdout.cc.i"
	cd /home/tysonv7/srsUE/build/ue/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/tysonv7/srsUE/ue/src/metrics_stdout.cc > CMakeFiles/ue.dir/metrics_stdout.cc.i

ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/ue.dir/metrics_stdout.cc.s"
	cd /home/tysonv7/srsUE/build/ue/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/tysonv7/srsUE/ue/src/metrics_stdout.cc -o CMakeFiles/ue.dir/metrics_stdout.cc.s

ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o.requires:
.PHONY : ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o.requires

ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o.provides: ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o.requires
	$(MAKE) -f ue/src/CMakeFiles/ue.dir/build.make ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o.provides.build
.PHONY : ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o.provides

ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o.provides.build: ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o

# Object files for target ue
ue_OBJECTS = \
"CMakeFiles/ue.dir/main.cc.o" \
"CMakeFiles/ue.dir/ue.cc.o" \
"CMakeFiles/ue.dir/metrics_stdout.cc.o"

# External object files for target ue
ue_EXTERNAL_OBJECTS =

ue/src/ue: ue/src/CMakeFiles/ue.dir/main.cc.o
ue/src/ue: ue/src/CMakeFiles/ue.dir/ue.cc.o
ue/src/ue: ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o
ue/src/ue: ue/src/CMakeFiles/ue.dir/build.make
ue/src/ue: ue/src/upper/libsrsue_upper.a
ue/src/ue: ue/src/common/libsrsue_common.a
ue/src/ue: ue/src/mac/libsrsue_mac.a
ue/src/ue: ue/src/phy/libsrsue_phy.a
ue/src/ue: ue/src/radio/libsrsue_radio.a
ue/src/ue: liblte/liblte.a
ue/src/ue: /usr/lib/x86_64-linux-gnu/libboost_program_options.so
ue/src/ue: /usr/lib/x86_64-linux-gnu/libboost_system.so
ue/src/ue: /usr/lib/x86_64-linux-gnu/libboost_date_time.so
ue/src/ue: /usr/lib/x86_64-linux-gnu/libboost_thread.so
ue/src/ue: /usr/lib/x86_64-linux-gnu/libpthread.so
ue/src/ue: /usr/lib/libpolarssl.so
ue/src/ue: ue/src/common/libsrsue_common.a
ue/src/ue: /usr/local/lib/libsrslte.so
ue/src/ue: /usr/local/lib/libsrslte_rf.so
ue/src/ue: ue/src/CMakeFiles/ue.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable ue"
	cd /home/tysonv7/srsUE/build/ue/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/ue.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
ue/src/CMakeFiles/ue.dir/build: ue/src/ue
.PHONY : ue/src/CMakeFiles/ue.dir/build

ue/src/CMakeFiles/ue.dir/requires: ue/src/CMakeFiles/ue.dir/main.cc.o.requires
ue/src/CMakeFiles/ue.dir/requires: ue/src/CMakeFiles/ue.dir/ue.cc.o.requires
ue/src/CMakeFiles/ue.dir/requires: ue/src/CMakeFiles/ue.dir/metrics_stdout.cc.o.requires
.PHONY : ue/src/CMakeFiles/ue.dir/requires

ue/src/CMakeFiles/ue.dir/clean:
	cd /home/tysonv7/srsUE/build/ue/src && $(CMAKE_COMMAND) -P CMakeFiles/ue.dir/cmake_clean.cmake
.PHONY : ue/src/CMakeFiles/ue.dir/clean

ue/src/CMakeFiles/ue.dir/depend:
	cd /home/tysonv7/srsUE/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/tysonv7/srsUE /home/tysonv7/srsUE/ue/src /home/tysonv7/srsUE/build /home/tysonv7/srsUE/build/ue/src /home/tysonv7/srsUE/build/ue/src/CMakeFiles/ue.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : ue/src/CMakeFiles/ue.dir/depend
