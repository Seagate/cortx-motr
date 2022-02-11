package simple

/*
#include <stdlib.h>
#include "lib/simple.h"
#cgo LDFLAGS: -L${SRCDIR}/lib -lsimple -L/home/502380/cortx-motr/motr/.libs -lmotr
*/
import "C"
import "unsafe"

type client struct {
	connected bool
}

func (c *client) Connect(ha_addr string, local_addr string, profile string, process_fid string) {

	ha := C.CString(ha_addr)
	lo := C.CString(local_addr)
	pro := C.CString(profile)
	process := C.CString(process_fid)

	defer C.free(unsafe.Pointer(ha))
	defer C.free(unsafe.Pointer(lo))
	defer C.free(unsafe.Pointer(pro))
	defer C.free(unsafe.Pointer(process))

	C.init(ha, lo, pro, process)

	c.connected = true
}

func (c *client) CreateIndex(id int64) int {

	if !c.connected {
		panic("client not connected to server")
	}

	index_id := C.longlong(id)

	rc := C.index_create(index_id)

	return int(rc)
}

func (c *client) DeleteIndex(id int64) int {

	if !c.connected {
		panic("client not connected to server")
	}

	index_id := C.longlong(id)

	rc := C.index_delete(index_id)

	return int(rc)
}
