#! /bin/bash
echo build.cmake >> .last-rebuild
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
module load aue/gcc/14.2.0
module load aue/openmpi/5.0.6-gcc-14.2.0
module load aue/boost/1.88.0-gcc-14.2.0-openmpi-5.0.6
module load cmake
cd cbuild.mpi
#VERBOSE=1 make && make install && make doc
VERBOSE=1 make && make install
