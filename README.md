# adct-cxx
Application Data Collection Toolkit for C++

- This is a library-only ADC c++ implementation for https://github.com/sandialabs/adct .
  - It depends at build time on boost::json headers, but exposes to the
    application code only c++17 standard dependencies.
  - A single factory class provides interface-only objects.
  - Documentation is generated during the build and installed.

- The blt source from https://github.com/LLNL/blt.git is required to build
  - check that https\_proxy is set as needed to reach github, if needed.
  - see INSTALL.md for a quick install procedure.

This libadc-cxx does or will soon:
- optionally depend on an MPI-capable compiler and MPI library.
- optionally depend on adiak from LLNL.
- optionally depend on libldms from ldms 4.5.1 or later.
- optionally depend on libcurl.

Development of comprehensive tutorial examples and feature tests is on-going.
- The current HPC example is examples/mpiSimpleDemo.cpp
- The primary coverage test is examples/demoBuilder.cpp.

Example build instructions are provided in scripts build.*:

- MPI
	./build.cmake
- No MPI
	./build.nompi.cmake
- LDMS / Adiak co-development
	./build.cmake.third

The build.\* scripts depend on site specific input parameters (see local-install-list)
that need changing at new sites or for new users.
The build.\* scripts depend on lmod module commands to select compiler tools; these
must be replaced with site-specific alternatives or site-specific \*PATH and
INCLUDE variables.

Code-based documentation is built with doxygen as part of a normal install.
It lands in $prefix/share/doc/adc/adc\_cxx/html/index.html
