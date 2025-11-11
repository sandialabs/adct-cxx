#! /bin/bash
# get the SERVER variable
. ./cpost.include.sh
# use the local input file with -d option
fn=./cpost.json
# write curl output to local scratch file.
log=$(mktemp)
curl -X POST \
  -s -w "\n%{http_code}\n" \
  -H "Content-Type: application/json" \
  -d @${fn} \
  https://$SERVER/log > $log
if grep collection $log; then
	echo found collection
	ret=$(head -n 2 $log |tail -n 1)
	echo $ret
else
	echo see $log for unexpected output
fi
