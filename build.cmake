#! /bin/bash
echo build.cmake >> .last-build
# load the PREFIX variables
# specific to your site
. local-install-list
# make sure 'module' exists; comment out if you remove
# the module commands below and set your own environment
# variables needed to get c++17.
. local-check-module
if test -z "$BLT_SRC"; then
	BLT_SRC=$(pwd)/../blt
fi
if test -z "$ADC_PREFIX"; then
	ADC_PREFIX=$(pwd)/inst-mpi
fi

module purge
module load aue/gcc/12.3.0
module load aue/openmpi/4.1.6-gcc-12.3.0
module load aue/boost/1.85.0-gcc-12.3.0-openmpi-4.1.6
mkdir -p cbuild.mpi
cd cbuild.mpi
cmake -DBLT_SOURCE_DIR=$BLT_SRC \
	-DBUILD_SHARED_LIBS=On \
	-DENABLE_MPI=On \
	-DENABLE_DOC=On \
	-DCMAKE_INSTALL_PREFIX=$ADC_PREFIX \
	-DCFLAGS=-g \
	-DCMAKE_BUILD_TYPE=Debug \
	-DENABLE_GTEST=Off \
       	..
VERBOSE=1 make && make install && make doc
