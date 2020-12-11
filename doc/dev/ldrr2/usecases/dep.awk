BEGIN { print "digraph {"; }

function norm(s) {
	gsub(/^[ \t]+/,"", s);
	gsub(/\./,"_", s);
	gsub(/[ +()-/]/,"_", s);
	return "_" s;
}
function complete() {
	print norm(id);
	split(depends, d, ", *");
	for (t in d) {
		print norm(id) " -> " norm(d[t]);
	}
}

function tail() {
	s = "";
	for (i = 2; i <= NF; i++) {
		s = s " " $i;
	}
	return s;
}

/:id:/             { complete(); id = substr($2, 2, length($2) - 2); }
/:name:/           { name      = tail(); }
/:author:/         { author    = tail(); }
/:detail:/         { detail    = tail(); }
/:justification:/  { j         = tail(); }
/:component:/      { component = tail(); }
/:req:/            { req       = tail(); }
/:process:/        { process   = tail(); }
/:depends:/        { depends   = tail(); }
/:resources:/      { resources = tail(); }
END { complete(); print "}"; }
