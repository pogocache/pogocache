package tests

import (
	"bytes"
	crand "crypto/rand"
	"io"
	"math/rand"
	"net"
	"net/http"
)

// randString returns random string with the random size of [0-n).
// All characters are alpha numeric.
func randString(n int) string {
	rchars := "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	str := make([]byte, rand.Int()%n)
	crand.Read(str)
	for i := range str {
		str[i] = rchars[int(str[i])%len(rchars)]
	}
	return string(str)
}

// randBytes returns random bytes with the random size of [0-n)
func randBytes(n int) string {
	str := make([]byte, rand.Int()%n)
	crand.Read(str)
	return string(str)
}

func httpDo(client *http.Client, method, uri, body string) (string, error) {
	req, err := http.NewRequest(method, "http://localhost:9401"+uri,
		bytes.NewBufferString(body))
	if err != nil {
		return "", err
	}
	if client == nil {
		client = http.DefaultClient
	}
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	bytes, err := io.ReadAll(resp.Body)
	return string(bytes), err
}

func shuffle[T any](slice []T) {
	for i := range slice {
		j := rand.Intn(i + 1)
		slice[i], slice[j] = slice[j], slice[i]
	}
}

func mcRawDo(mc net.Conn, packet string) (string, error) {
	n, err := mc.Write([]byte(packet))
	if err != nil {
		return "", err
	}
	if n != len(packet) {
		return "", io.ErrShortWrite
	}
	var buf [4096]byte
	n, err = mc.Read(buf[:])
	if err != nil {
		return "", err
	}
	resp := string(buf[:n])
	return resp, nil
}
