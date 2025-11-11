
Installation instructions for Application Data Collection Toolkit (c++) on Linux

- Create an new directory and change into it. E.g.
```
mkdir adc
cd adc
```

- Get the required sources:
```
git clone https://github.com/LLNL/blt.git
git clone https://github.com/sandialabs/adct-cxx.git
```

- Make sure that boost c++ headers are installed so that boost.json is available.
The details of this vary by platform.
Boost 1.85 is known to work.
Boost is only required to build the library, not to use it.

- Basic build with cmake and a c++17 compiler

GCC 12 and later are known to be adequately c++17 compliant.

Scripts providing example invocations of cmake come with adct-cxx.
The simplest is adct-cxx/build.nompi.cmake. It will require local
tailoring in most HPC environments.
```
cd adct-cxx
<your-favorite-editor> build.nompi.cmake
```
If your default compiler supports c++17 and the corresponding
standard c++ library, simply comment out
all lines in which 'module' appears anywhere.


- MPI build

GCC 12 and later are known to be adequately c++17 compliant.

Scripts providing example invocations of cmake come with adct-cxx.
The MPI build example is adct-cxx/build.cmake. It will require local
tailoring in most HPC environments.
```
cd adct-cxx
<your-favorite-editor> build.cmake
```
If your default compiler supports c++17 and the corresponding
standard c++ library and MPI, simply comment out
all lines in which 'module' appears anywhere.

