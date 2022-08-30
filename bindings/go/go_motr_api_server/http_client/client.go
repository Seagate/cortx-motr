package http_client

import (
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"net/http"
	"net/url"
	"os"
)

type Result struct {
	Status  string
	IndexID string `json:"index_id,omitempty"`
	Key     string
	Value   string
	Error   string
}

type Client struct {
	host string
	port int
}

func NewClient(host string, port int) Client {

	ic := Client{}
	ic.host = host
	ic.port = port

	return ic
}

func (ic Client) Status() (*Result, error) {
	return ic.get("status")
}

func (ic Client) Put(indexID, key, value string) (*Result, error) {
	return ic.post("kv/put", url.Values{"index_id": {indexID}, "key": {key}, "value": {value}})
}

func (ic Client) Get(indexID, key string) (*Result, error) {
	return ic.get("kv/get?index_id=" + indexID + "&key=" + key)
}

func (ic Client) Delete(indexID, key string) (*Result, error) {
	return ic.post("kv/delete", url.Values{"index_id": {indexID}, "key": {key}})
}

func (ic Client) url() string {
	return "http://" + ic.host + ":" + toString(ic.port)
}

func (ic Client) get(url string) (*Result, error) {
	//fmt.Println(ic.url() + "/" + url)

	res, err := http.Get(ic.url() + "/" + url)

	if err != nil {
		return nil, err
	}

	defer res.Body.Close()

	if res.StatusCode != http.StatusOK {
		result, _ := unMarshall(res)
		return nil, errors.New(res.Status + ":" + result.Error)
	}

	return unMarshall(res)
}

func (ic Client) post(url string, values url.Values) (*Result, error) {
	//fmt.Println(ic.url()+"/"+url, values)

	res, err := http.PostForm(ic.url()+"/"+url, values)

	if err != nil {
		return nil, err
	}

	defer res.Body.Close()

	if res.StatusCode != http.StatusOK {
		result, _ := unMarshall(res)
		return nil, errors.New(res.Status + ":" + result.Error)
	}

	return unMarshall(res)
}

// object

func (ic Client) Write(indexID, file string) (*Result, error) {

	reader, err := os.Open(file)

	if err != nil {
		return nil, err
	}

	info, err := os.Stat(file)

	if err != nil {
		return nil, err
	}

	url := ic.url() + "/object/write?index_id=" + indexID + "&size=" + toString(info.Size())

	res, err := http.Post(url, "application/octet-stream", reader)

	if err != nil {
		return nil, err
	}

	if res.StatusCode != http.StatusOK {
		result, _ := unMarshall(res)
		return nil, errors.New(res.Status + ":" + result.Error)
	}

	return unMarshall(res)
}

func (ic Client) Read(indexID string, size int, fileName string) error {

	url := ic.url() + "/object/read?index_id=" + indexID + "&size=" + toString(size)

	res, err := http.Get(url)

	if err != nil {
		return err
	}

	if res.StatusCode != http.StatusOK {
		result, _ := unMarshall(res)
		return errors.New(res.Status + ":" + result.Error)
	}

	body, err := ioutil.ReadAll(res.Body)

	if err != nil {
		return err
	}

	err = ioutil.WriteFile(fileName, body, 0644)

	if err != nil {
		return err
	}

	return nil
}

func unMarshall(res *http.Response) (*Result, error) {

	body, err := ioutil.ReadAll(res.Body)

	if err != nil {
		return nil, err
	}

	//fmt.Println(string(body))

	result := Result{}
	json.Unmarshal(body, &result)

	return &result, nil
}

func toString(t interface{}) string {
	return fmt.Sprintf("%+v", t)
}
