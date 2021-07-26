// waysct is a set color temp implementation for Wayland.
// Many of these files are taken from
// https://github.com/minus7/redshift/commit/7da875d34854a6a34612d5ce4bd8718c32bec804
// see Redshift for the GPL license.
package waysct

//go:generate wayland-scanner private-code orbital-authorizer.xml orbital-authorizer-protocol.h
//go:generate wayland-scanner client-header orbital-authorizer.xml orbital-authorizer-client-protocol.h
//go:generate wayland-scanner private-code gamma-control.xml gamma-control-protocol.h
//go:generate wayland-scanner client-header gamma-control.xml gamma-control-client-protocol.h

// #cgo LDFLAGS: -lm -lwayland-client
// #include "gamma-wl.h"
import "C"
import (
	"github.com/pkg/errors"
)

type Manager struct {
	state *C.wayland_state_t
}

func StartManager() (*Manager, error) {
	m := Manager{}
	if errno := C.wayland_init(&m.state); errno != 0 {
		return nil, errors.Errorf("wayland_init: errno %d", errno)
	}
	if errno := C.wayland_start(m.state); errno != 0 {
		return nil, errors.Errorf("wayland_start: errno %d", errno)
	}
	return &m, nil
}

func (m *Manager) Close() {
	C.wayland_free(m.state)
	m.state = nil
}

func (m *Manager) SetColorTemp(temp int) error {
	var setting C.color_setting_t
	setting.brightness = 1.0
	setting.gamma[0] = 1.0
	setting.gamma[1] = 1.0
	setting.gamma[2] = 1.0
	setting.temperature = C.int(temp)
	if errno := C.wayland_set_temperature(m.state, &setting); errno != 0 {
		return errors.Errorf("wayland_set_temperature: errno %d", errno)
	}
	return nil
}
