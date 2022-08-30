package http_client

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

const IndexID = "0x7800000000000123:1"
const IndexID_object = "0x7800000000000123:3"

var client Client = NewClient("localhost", 8081)

func TestStatus(t *testing.T) {
	res, err := client.Status()

	assert.Nil(t, err)
	assert.Equal(t, "ok", res.Status)
}

func TestPut(t *testing.T) {
	res, err := client.Put(IndexID, "storage", "motr")

	assert.Nil(t, err)

	if res != nil {
		assert.Equal(t, "motr", res.Value)
	}
}

func TestGet(t *testing.T) {

	res, err := client.Get(IndexID, "storage")

	assert.Nil(t, err)

	if res != nil {
		assert.Equal(t, "motr", res.Value)
	}
}

func TestDelete(t *testing.T) {
	res, err := client.Delete(IndexID, "storage")

	assert.Nil(t, err)

	if res != nil {
		assert.Equal(t, "storage", res.Key)
	}
}

func TestWrite(t *testing.T) {

	_, err := client.Write(IndexID_object, "black_hole.png")

	assert.Nil(t, err)
}

func TestRead(t *testing.T) {

	err := client.Read(IndexID_object, 68277, "out.png")

	assert.Nil(t, err)
}

func BenchmarkRead(t *testing.B) {

	err := client.Read(IndexID_object, 68277, "out.png")

	assert.Nil(t, err)
}
