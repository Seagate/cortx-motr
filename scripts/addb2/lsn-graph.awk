#! /usr/bin/gawk -f

#
# Takes output of addb2dump as input.
#

function trim(s)
{
	return substr(s, 1, length(s) - 1);
}

#
# Knuth's hash function
#
function hash(key)
{
	return key * 2654435761 % 2^32
}

function tcolor(tid)
{
	return sprintf("\"#%02.2x%02.2x%02.2x\"", hash(tid) % 256,
		       hash(tid + 1) % 256, hash(tid + 2) % 256);
}

BEGIN {
	print "digraph dep {";
	print "    graph [rankdir=TB]"
	print "    node [shape=record];"
}

{
# 1 2            3            4    5   6    7    8     9             10    11
# * 40.461453170 tx-capture   tid: 0,  lsn: 33,  addr: 4000000006a8, size: 56

	if ($3 != "tx-capture")
		next
	mem[trim($9)][trim($7)] = trim($5)
}

END {
	for (adr in mem) {
		print "    { rank=same a" adr
		for (lsn in mem[adr]) {
			print "        l" lsn
		}
		print "    }"
	}
	for (adr in mem) {
		first = 1
		print "    a" adr " [ shape=cds label=\"" adr "\" ]"
		for (lsn in mem[adr]) {
			if (first == 1) {
				print "    a" adr " -> l" lsn
				first = 0
			}
			print "    l" lsn "[color=" tcolor(mem[adr][lsn]) "]"
			" label=\"" lsn "\"]"
			print "    l" lsn " -> l" lsn + 1
		}
	}
	print "}";
}
