

# * 2021-09-04-04:02:10.033034618 sock-epoll-wait   nr: 161 min: 2 max: 979421 avg: 322681.993789 dev: 210484288639.211151 datum: 0  0 0: 40 81618: 0 163236: 0 244854: 0 326472: 0 408090: 0 489708: 0 571326: 0 652944: 0 734562: 0 816180: 0 897798: 21 979416: 0

BEGIN {
	NN = 14
	nr = 0
	min = 0
	max = 0
	ts = ""
	label = ""

	for (i = 0; i < NN; ++i) {
		b[i] = 0
		l[i] = "0:"
	}
}

END {
	tot = 0
	for (i = 0; i < NN; ++i) {
		tot += b[i]
	}
	printf "* %s %s nr: %i min: %i max: %i avg: %f dev: 0.0 datum: 0 ",
		ts, label, nr, min, max, avg;
	for (i = 0; i < NN; ++i) {
		printf "%i %s ", b[i], l[i]
		tot += b[i]
	}
	printf "\n"
}

{
	if ($1 != "*") {
		printf "%s: Must start with *\n", $0
		exit 1
	}
	if (NF != 42) {
		printf "%s: Must be exactly 42 field, got %i\n", $0, NF
		exit 1
	}
	if (label == "") {
		label = $3
	}
	if (label != $3) {
		printf "%s: label mismatch at '%s' != '%s'\n", $0, label, $3
		exit 1
	}
	if (ts < $2) {
		ts = $2
	}
	nr += $5
	if ($7 < min) {
		min = $7
	}
	if ($9 > max) {
		max = $9
	}
	for (i = 0; i < NN; ++i) {
		ll = $(17 + 2*i)
		b[i] += $(16 + 2*i)
		if (l[i] == "0:") {
			l[i] = ll
		}
		if (l[i] != ll && ll != "0:") {
			printf "%s: bucket mismatch at %i: '%s' != '%s'\n",
				$0, i, l[i], ll
			exit 1;
		}
	}
}
