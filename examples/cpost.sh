#! /bin/bash
# get the SERVER variable
. ./cpost.include.sh
# use the local input file with -d option
uuid=$(uuid -v4)
uid=$(id -u)
etime=$(date +%s.%N)
edate=$(date +%FT%X.%N%Z)
cat cpost.json.in | sed -e "s/@USER@/$USER/g" \
	-e "s/@TIME@/$etime/g"  \
	-e "s/@DATE@/$edate/g"  \
	-e "s/@UID@/$uid/g"  \
	-e "s/@UUID@/$uuid/g"  \
> ./cpost.json.tmp
fn=./cpost.json.tmp
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
# Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
#
# SPDX-License-Identifier: BSD-3-Clause
#
