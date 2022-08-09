package app

import (
	"io/ioutil"

	"github.com/BurntSushi/toml"
	"github.com/Seagate/cortx-motr/bindings/go/mio"
)

var (
	Conf Config
)

type Config struct {
	Address string

	Ha_addr     string
	Local_addr  string
	Profile     string
	Process_fid string
}

func init() {

	data, err := ioutil.ReadFile("motr.toml")

	if err != nil {
		panic(err.Error())
	}

	if _, err := toml.Decode(string(data), &Conf); err != nil {
		panic(err.Error())
	}

	motr()
}

func motr() {
	mio.Init(&Conf.Local_addr, &Conf.Ha_addr, &Conf.Profile, &Conf.Process_fid)
}
