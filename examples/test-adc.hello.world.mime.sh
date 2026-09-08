#! /bin/bash
module purge
module load aue/gdb
if ! test -f ./test-env-publishers; then
	echo run test while current directory is $repo/examples
	exit 1
fi
. ./test-env-publishers
export ADC_MULTI_PUBLISHER_DEBUG=1
export ADC_MULTIFILE_PLUGIN_DEBUG=1
echo starting...
#valgrind -v --leak-check=full --show-leak-kinds=all --log-file=mime.leaks ../inst-mpi/bin/adc.hello.world.mime
../inst-mpi/bin/adc.hello.world.mime
# expect: /dev/shm/adc outputs
