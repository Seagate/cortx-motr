#! /usr/bin/awk

function trim(s)
{
	return substr(s, 1, length(s) - 1);
}

{
	tid = trim($5);
	lsn = trim($7);
	adr = trim($9);
	siz = 0 + $9;
	if (lsn in trid) {
		printf "Duplicate lsn: %i\n", lsn;
		exit 1
	} else {
		trid[lsn] = tid;
		addr[lsn] = adr;
		size[lsn] = siz;
		if (adr in mem) {
			olsn = mem[adr]
			otid = trid[olsn];
			if (otid != tid) {
				printf "c %i [%i] -> %i [%i]\n",
					otid, olsn, tid, lsn;
				if ((otid, tid) in cnfl) {
					cnfl[otid, tid] += 1;
				} else {
					cnfl[otid, tid] = 1;
					printf "C %i -> %i\n", otid, tid;
				}
			}
			if (size[olsn] != siz) {
				printf "s %s %i [%i] -> %i [%i]\n",
					adr, size[olsn], olsn, siz, lsn;
			}
		}
		mem[adr] = lsn;
		ww[adr][tid] += 1;
		if (!(tid in tran)) {
			printf "T %i [%i]\n", tid, lsn;
			tran[tid] = lsn;
		}
	}
}

END {
	for (a in ww) {
		t = 0;
		for (tid in ww[a]) {
			printf "w %s %i %i\n", a, tid, ww[a][tid];
			t += ww[a][tid];
		}
		printf "W %s %i\n", a, t;
	}
}
