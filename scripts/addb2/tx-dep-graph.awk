#! /usr/bin/awk

#
# Takes output of capture.awk as input.
#

BEGIN {
	print "digraph dep {";
}

END {
	print "}";
}

{
	if ($1 == "T") {
		printf "        %s\n", $2;
	}
	if ($1 == "C") {
		printf "        %s -> %s\n", $2, $4;
	}
}
