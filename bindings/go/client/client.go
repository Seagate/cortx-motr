/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */

package client

/*
#cgo CFLAGS: -I../../.. -I/usr/include/motr
#cgo CFLAGS: -DM0_EXTERN=extern -DM0_INTERNAL=
#cgo CFLAGS: -Wno-attributes
#cgo LDFLAGS: -L../../../motr/.libs -Wl,-rpath=../../../motr/.libs -lmotr

#include <stdlib.h>
#include "motr/client.h"
#include "lib/trace.h"

struct m0_config    			conf;
struct m0_idx_dix_config 	dix_conf;

struct m0_client    		*instance;
struct m0_container 		container;

*/
import "C"

import (
	"context"
	"cortx-motr/bindings/go/mio"
	"errors"
	"fmt"
	"io"
	"log"
	"os"
	"strconv"
	"unsafe"
)

/*
Options is for initialize motr client
*/
type Options struct {
	HAAddr           string
	LocalAddr        string
	Profile          string
	ProcessFId       string
	TmRecvQeueMinLen int
	MaxRPCMsgSize    int
	KcCreateMeta     bool
	TraceOn          bool

	// MakeIsOOStoreFalse is a control flag for IsOOStore
	IsOOStore, MakeIsOOStoreFalse bool
}

/*
This init default options.
*/
func (opt *Options) init() {

	opt.IsOOStore = !opt.MakeIsOOStoreFalse

	if opt.TmRecvQeueMinLen == 0 {
		opt.TmRecvQeueMinLen = C.M0_NET_TM_RECV_QUEUE_DEF_LEN
	}

	if opt.MaxRPCMsgSize == 0 {
		opt.MaxRPCMsgSize = C.M0_RPC_DEF_MAX_RPC_MSG_SIZE
	}

}

/*
Client represent motr client.
*/
type Client struct {
	connected bool
}

/*
NewClient create new client instance.
*/
func NewClient(opt *Options) (*Client, error) {

	opt.init()

	c := Client{}
	c.cConfOptions(opt)

	rc := c.cInitClient()

	if rc != 0 {
		return nil, fmt.Errorf("fail to initial client, error code = %d", rc)
	}

	c.connected = true

	return &c, nil
}

/*
This method to set Options to C struct m0_config.
*/
func (c *Client) cConfOptions(opt *Options) {

	C.dix_conf.kc_create_meta = C.bool(opt.KcCreateMeta)
	C.conf.mc_idx_service_conf = unsafe.Pointer(&C.dix_conf)

	C.conf.mc_is_oostore = C.bool(opt.IsOOStore)
	C.conf.mc_local_addr = C.CString(opt.LocalAddr)
	C.conf.mc_ha_addr = C.CString(opt.HAAddr)
	C.conf.mc_profile = C.CString(opt.Profile)
	C.conf.mc_process_fid = C.CString(opt.ProcessFId)
	C.conf.mc_tm_recv_queue_min_len = C.uint(opt.TmRecvQeueMinLen)
	C.conf.mc_max_rpc_msg_size = C.uint(opt.MaxRPCMsgSize)
	C.conf.mc_idx_service_id = C.M0_IDX_DIX

	if !opt.TraceOn {
		C.m0_trace_set_mmapped_buffer(false)
		C.m0_trace_level_allow(C.M0_WARN)
	}
}

/*
Initialize client by invoke C methods.
*/
func (c *Client) cInitClient() int {

	rc := C.m0_client_init(&C.instance, &C.conf, true)

	if rc != 0 {
		log.Panicf("m0_client_init() failed: %v", rc)
	}

	C.m0_container_init(&C.container, nil, &C.M0_UBER_REALM, C.instance)
	rc = C.container.co_realm.re_entity.en_sm.sm_rc

	if rc != 0 {
		log.Panicf("m0_container_init() failed: %v", rc)
	}

	return int(rc)
}

/*
IsConnected check if client is connect to motr.
*/
func (c *Client) IsConnected() bool {
	return c.connected
}

/*
Create a new index.
*/
func (c *Client) Create(ctx context.Context, indexID string) error {

	var mkv mio.Mkv
	defer mkv.Close()

	err := mkv.Open(indexID, true)

	if err != nil {
		return err
	}

	return nil
}

/*
Put key value pairs to index.
*/
func (c *Client) Put(ctx context.Context, indexID string, key, value string, keyValues ...string) error {

	var mkv mio.Mkv
	defer mkv.Close()

	if err := mkv.Open(indexID, false); err != nil {
		return err
	}

	if err := mkv.Put([]byte(key), []byte(value), true); err != nil {
		return err
	}

	var length = len(keyValues)

	// check if key value is pairs
	if length%2 != 0 {
		return errors.New("not key value pair")
	}

	for i := 0; i < length/2; i++ {

		key := keyValues[(i * 2)]
		val := keyValues[(i*2)+1]

		if err := mkv.Put([]byte(key), []byte(val), true); err != nil {
			return err
		}
	}

	return nil
}

/*
Get values from index by multiple keys.
*/
func (c *Client) Get(ctx context.Context, indexID string, key string, keys ...string) (map[string]string, error) {

	var mkv mio.Mkv
	defer mkv.Close()

	if err := mkv.Open(indexID, false); err != nil {
		return nil, err
	}

	b, err := mkv.Get([]byte(key))

	if err != nil {
		return nil, err
	}

	var m = make(map[string]string)

	m[key] = string(b)

	for _, k := range keys {

		b, err := mkv.Get([]byte(k))

		if err != nil {
			return nil, err
		}

		m[k] = string(b)
	}

	return m, nil
}

/*
Delete key from index.
*/
func (c *Client) Delete(ctx context.Context, indexID string, key string, keys ...string) error {

	var mkv mio.Mkv
	defer mkv.Close()

	if err := mkv.Open(indexID, false); err != nil {
		return err
	}

	if err := mkv.Delete([]byte(key)); err != nil {
		return err
	}

	for _, k := range keys {

		err := mkv.Delete([]byte(k))

		if err != nil {
			return err
		}
	}

	return nil
}

// File I/O

func openFile(src string) (io.ReadCloser, uint64, error) {

	file, err := os.Open(src)

	if err != nil {
		return nil, 0, err
	}

	info, err := file.Stat()

	if err != nil {
		return nil, 0, err
	}

	objSize := uint64(info.Size())

	return file, objSize, nil
}

func writeBuf(bufSize uint64, writer io.Writer, reader io.Reader) (int64, error) {

	buf := make([]byte, bufSize)
	// Force Writer to always use buf.(Otherwise, the performance
	// might suffer. A tip from https://github.com/golang/go/issues/16474)
	written, err := io.CopyBuffer(struct{ io.Writer }{writer}, reader, buf)

	return written, err
}

func (c *Client) writeFile(ctx context.Context, key string, sourceFile string) (uint64, error) {

	reader, objSize, err := openFile(sourceFile)

	if err != nil {
		return 0, err
	}

	defer reader.Close()

	_, err = mio.ScanID(key)

	if err != nil {
		return 0, err
	}

	var mioW mio.Mio
	if err = mioW.Open(key); err != nil {
		if err = mioW.Create(key, objSize, ""); err != nil {
			return 0, fmt.Errorf("failed to create object %v: %v", sourceFile, err)
		}
	}

	if err != nil {
		return 0, err
	}

	defer mioW.Close()

	_, err = writeBuf(objSize, &mioW, reader)

	if err != nil {
		return 0, err
	}

	return objSize, nil
}

/*
Write file to index, objectId is file objectId, and file is value to write to.
*/
func (c *Client) Write(ctx context.Context, indexID string, objectID string, file string) error {

	objectSize, err := c.writeFile(ctx, objectID, file)

	if err != nil {
		return err
	}

	err = c.Create(ctx, indexID)

	if err != nil {
		return err
	}

	size := strconv.FormatUint(objectSize, 10)
	err = c.Put(ctx, indexID, "size", size, "file", objectID)

	if err != nil {
		return err
	}

	return nil
}

func (c *Client) readFile(ctx context.Context, src string, objSize uint64, dst string) error {

	_, err := mio.ScanID(src)

	if err != nil {
		return err
	}

	var mioR mio.Mio
	if err = mioR.Open(src, objSize); err != nil {
		return fmt.Errorf("failed to open object %v: %v", src, err)
	}
	defer mioR.Close()
	reader := &mioR

	file, err := os.OpenFile(dst, os.O_WRONLY|os.O_CREATE, 0644)
	if err != nil {
		return fmt.Errorf("failed to open file %v: %v", dst, err)
	}
	defer file.Close()
	writer := file

	if err != nil {
		return err
	}
	defer writer.Close()

	_, err = writeBuf(objSize, writer, reader)

	if err != nil {
		return err
	}

	return nil
}

/*
Read file from indexId, content will write to outfile.
*/
func (c *Client) Read(ctx context.Context, indexID string, outfile string) error {

	props, err := c.Get(ctx, indexID, "size", "file")

	if err != nil {
		return err
	}

	size, _ := strconv.ParseUint(props["size"], 10, 64)

	err = c.readFile(ctx, props["file"], size, outfile)

	if err != nil {
		return err
	}

	return nil
}
