#! /bin/bash
module purge
module load aue/gdb
rm -rf test.outputs adc.file_plugin.log
set -x
. ./test-env-publishers
set +x
export ADC_MULTI_PUBLISHER_DEBUG=1
export ADC_MULTIFILE_PLUGIN_DEBUG=1
echo starting...
#valgrind -v --leak-check=full --show-leak-kinds=all ../inst-mpi/bin/adc.hello.world.auto
../inst-mpi/bin/adc.hello.world.auto
if test -s ./adc.file_plugin.log; then
	echo "file plugin ok"
else
	 echo "missing adc.file_plugin.log"
fi
sleep 1 ; # catch up with nfs delays
if cat ./test.outputs/script; then
	:
else
	 echo "missing test.outputs/script"
fi
# expect: /projects/adc/baallan outputs
