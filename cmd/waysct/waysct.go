// waysct is a set color temp utility for Wayland.
package main

import (
	"log"

	"github.com/d4l3k/go-sct/sctcli"
	"github.com/d4l3k/go-sct/waysct"
)

func main() {
	m, err := waysct.StartManager()
	if err != nil {
		log.Fatalf("%+v", err)
	}
	defer m.Close()
	sctcli.Main(m.SetColorTemp)
}
