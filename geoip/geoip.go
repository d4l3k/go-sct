// geoip returns the lat/lng of the target IP address (or current machine)
package geoip

import (
	"encoding/json"
	"fmt"
	"net/http"
)

type GeoIP struct {
	City      string  `json:"city"`
	Latitude  float64 `json:"lat"`
	Longitude float64 `json:"lon"`
}

// LookupIP looks up the geolocation information for the specified address ("" for current host).
func LookupIP(address string) (*GeoIP, error) {
	response, err := http.Get(fmt.Sprintf("http://ip-api.com/json/%s?fields=lat,lon,city", address))
	if err != nil {
		return nil, err
	}
	defer response.Body.Close()

	var geo GeoIP
	if err := json.NewDecoder(response.Body).Decode(&geo); err != nil {
		return nil, err
	}
	return &geo, nil
}
