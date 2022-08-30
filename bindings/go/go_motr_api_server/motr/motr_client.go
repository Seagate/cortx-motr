package motr

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"log"

	"github.com/Seagate/cortx-motr/bindings/go/mio"
)

/*

This is wrapper of motr go bindings mio client

*/

func Get(indexID, key string) (string, error) {

	var mkv mio.Mkv

	if err := mkv.Open(indexID, false); err != nil {
		return "", fmt.Errorf("failed to open index %v: %v", indexID, err)
	}

	defer mkv.Close()

	value, err := mkv.Get([]byte(key))

	if err != nil {
		return "", fmt.Errorf("failed to get: %v", err)
	}

	return string(value), nil
}

func Delete(indexID, key string) error {

	var mkv mio.Mkv

	if err := mkv.Open(indexID, false); err != nil {
		return fmt.Errorf("failed to open index %v: %v", indexID, err)
	}

	defer mkv.Close()

	err := mkv.Delete([]byte(key))

	if err != nil {
		return fmt.Errorf("failed to delete: %v", err)
	}

	return err
}

func Put(indexID, key, value string) error {

	var mkv mio.Mkv

	if err := mkv.Open(indexID, true); err != nil {
		return fmt.Errorf("failed to open index %v: %v", indexID, err)
	}

	defer mkv.Close()

	err := mkv.Put([]byte(key), []byte(value), true)

	if err != nil {
		return fmt.Errorf("failed to put: %v", err)
	}

	return err
}

func Write(pool string, indexID string, size uint64, reader io.Reader) error {

	var mioW mio.Mio
	defer mioW.Close()

	if _, err := mio.ScanID(indexID); err == nil {
		if pool != "" {
			if _, err := mio.ScanID(pool); err != nil {
				return fmt.Errorf("invalid pool specified: %v", pool)
			}
		}
		if err = mioW.Open(indexID); err != nil {
			if err = mioW.Create(indexID, size, pool); err != nil {
				return fmt.Errorf("failed to create object %v: %v", indexID, err)
			}
		}

		if pool != "" && !mioW.InPool(pool) {
			return fmt.Errorf("the object already exists in another pool: %v", mioW.GetPool())
		}

		if _, err := mioW.Seek(0, io.SeekStart); err != nil {
			return fmt.Errorf("failed to set offset (%v) for object %v: %v", 0, indexID, err)
		}
	}

	_, err := writeBuf(size, &mioW, reader)

	if err != nil {
		return err
	}

	return nil
}

func Read(pool string, indexID string, size uint64) ([]byte, error) {

	var mioR mio.Mio
	defer mioR.Close()

	if _, err := mio.ScanID(indexID); err == nil {
		if err = mioR.Open(indexID, size); err != nil {
			log.Fatalf("failed to open object %v: %v", indexID, err)
		}
	}

	var b bytes.Buffer
	writer := bufio.NewWriter(&b)

	_, err := writeBuf(size, writer, &mioR)

	if err != nil {
		return nil, err
	}

	return b.Bytes(), nil
}

func writeBuf(bufSize uint64, dst io.Writer, src io.Reader) (int64, error) {

	// Force Writer to always use buf.(Otherwise, the performance
	// might suffer. A tip from https://github.com/golang/go/issues/16474)
	return io.CopyBuffer(struct{ io.Writer }{dst}, src, make([]byte, bufSize))
}
