package simple

import "testing"

var motrClient = client{}

func init() {
	motrClient.Connect("192.168.54.176@tcp:12345:34:1", "192.168.54.176@tcp:12345:33:1000", "<0x7000000000000001:0>", "<0x7200000000000001:64>")
}

func TestCreateIndex(t *testing.T) {

	id := int64(1234567)

	rc := motrClient.CreateIndex(id)

	if rc != 0 {
		t.Fatalf(`CreateIndex("") = %q, want "0", error`, rc)
	}
}

func TestDeleteIndex(t *testing.T) {

	id := int64(1234567)

	rc := motrClient.DeleteIndex(id)

	if rc != 0 {
		t.Fatalf(`DeleteIndex("") = %q, want "0", error`, rc)
	}
}
