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

import (
	"context"
	"fmt"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

const indexID = "0x7800000000000123:0"
const objectID = "0x7800000000000124:0"

const imageID = "0x7800000000000125:0"
const imageObjectID = "0x7800000000000126:0"

var _size uint64

var client *Client
var ctx context.Context = context.Background()

func TestInit(t *testing.T) {

	start := time.Now()

	var err error
	client, err = NewClient(&Options{
		HAAddr:     "192.168.63.52@tcp:12345:34:1",
		LocalAddr:  "192.168.63.52@tcp:12345:33:1000",
		Profile:    "0x7000000000000001:0",
		ProcessFId: "0x7200000000000001:64",
		TraceOn:    false,
	})

	end := time.Since(start)

	fmt.Printf("connect = %v \n", end)
	assert.Nil(t, err)
	assert.NotNil(t, client)
}

func TestConnected(t *testing.T) {

	connected := client.IsConnected()

	assert.True(t, connected)
}

func TestCreate(t *testing.T) {

	start := time.Now()

	// currently no api to test is index exist
	err := client.Create(ctx, indexID)

	end := time.Since(start)

	fmt.Printf("create use = %v \n", end)

	assert.Nil(t, err)
}

func TestPut(t *testing.T) {

	start := time.Now()

	err := client.Put(ctx, indexID, "k1", "v1", "k2", "v2", "k3", "v3", "k4", "v4")

	end := time.Since(start)

	fmt.Printf("put use = %v \n", end)

	assert.Nil(t, err)
}

func TestGet(t *testing.T) {

	start := time.Now()

	keyValues, err := client.Get(ctx, indexID, "k1", "k2", "k3", "k4")

	end := time.Since(start)

	fmt.Printf("get use = %v \n", end)

	assert.Nil(t, err)
	assert.True(t, len(keyValues) == 4)
}

func TestDelete(t *testing.T) {

	start := time.Now()

	err := client.Delete(ctx, indexID, "k1", "k2")

	end := time.Since(start)

	fmt.Printf("delete use = %v \n", end)

	assert.Nil(t, err)
}

func TestWriteFile(t *testing.T) {

	start := time.Now()

	s, err := client.writeFile(ctx, objectID, "test_image.jpg")

	_size = s

	end := time.Since(start)

	fmt.Printf("Write File use = %v, size = %d bytes \n", end, _size)

	assert.Nil(t, err)
}

func TestReadFile(t *testing.T) {

	start := time.Now()

	err := client.readFile(ctx, objectID, _size, "1.jpg")

	end := time.Since(start)

	fmt.Printf("Read File use = %v \n", end)

	assert.Nil(t, err)
}

func TestWrite(t *testing.T) {

	start := time.Now()

	err := client.Write(ctx, imageID, imageObjectID, "test_image.jpg")

	end := time.Since(start)

	fmt.Printf("Write with image id = %v \n", end)

	assert.Nil(t, err)
}

func TestRead(t *testing.T) {

	start := time.Now()

	err := client.Read(ctx, imageID, "2.jpg")

	end := time.Since(start)

	fmt.Printf("Read with image id = %v \n", end)

	assert.Nil(t, err)
}

// negative test

func TestGetMissKey(t *testing.T) {
	_, err := client.Get(ctx, indexID, "key")

	// fmt.Printf("%v \n", err)
	assert.NotNil(t, err, "%v", err)
}

func TestPutNotKeyPair(t *testing.T) {
	err := client.Put(ctx, indexID, "k1", "k2", "key")

	// fmt.Printf("%v \n", err)
	assert.NotNil(t, err)
}
