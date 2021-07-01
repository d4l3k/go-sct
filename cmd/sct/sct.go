package main

import (
	"github.com/d4l3k/go-sct"
	"github.com/d4l3k/go-sct/sctcli"
)

func main() {
	sctcli.Main(func(temp int) error {
		sct.SetColorTemp(temp)
		return nil
	})
}
