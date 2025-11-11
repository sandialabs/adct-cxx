#! /bin/bash
# This is example input for the adc script_plugin publisher.
#
# arguments: name of a file containint one json object
#
# requirements:
# This script can do anything else the user wants, but the
# last thing it must do is delete the input file given.
if test "x$1" = "x"; then
	echo "expected json input filename" 1>&2
	exit 1
fi
json_reformat < $1 >> test.outputs/script
/bin/rm $1
