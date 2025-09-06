package tests

import (
	"fmt"
	"net"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/bradfitz/gomemcache/memcache"
	"github.com/stretchr/testify/assert"
)

func TestMemcache(t *testing.T) {
	// mc := memcache.New("127.0.0.1:11211")
	mc := memcache.New("127.0.0.1:9401")
	if err := mc.FlushAll(); err != nil {
		t.Fatal(err)
	}
	defer mc.Close()

	keys := make(map[string]string)
	for len(keys) < 10000 {
		key := randString(128)
		if len(key) > 0 {
			keys[key] = randString(1024)
		}
	}

	var count int
	for key, val := range keys {
		err := mc.Set(&memcache.Item{Key: key, Value: []byte(val)})
		if err != nil {
			t.Fatal(err)
		}
		count++
		item, err := mc.Get(key)
		if err != nil {
			t.Fatal(err)
		}
		if string(item.Value) != val {
			t.Fatalf("expected %s, got %s\n", val, item.Value)
		}
	}

	err := mc.Set(&memcache.Item{Key: "", Value: nil})
	if err == nil {
		t.Fatalf("expected error got nil\n")
	}

	for key := range keys {
		err := mc.Delete(key)
		if err != nil {
			t.Fatal(err)
		}
		count--
		item, err := mc.Get(key)
		if err == nil || item != nil {
			t.Fatalf("expected error got nil\n")
		}
	}

}

func TestMemcacheIssue15(t *testing.T) {
	addr := "127.0.0.1:9401"
	// addr := "127.0.0.1:11211"

	var wg sync.WaitGroup
	worker := func(id int) {
		defer wg.Done()
		mc, err := net.Dial("tcp", addr)
		assert.Nil(t, err)
		defer mc.Close()

		increment := func() bool {
			resp, err := mcRawDo(mc, "gets counter\r\n")
			assert.Nil(t, err)
			cas := strings.Split(strings.Split(resp, "\r\n")[0], " ")[4]
			value := strings.Split(strings.Split(resp, "\r\n")[1], "\r\n")[0]
			ivalue, _ := strconv.Atoi(value)
			ivalue++
			value = fmt.Sprint(ivalue)
			resp, err = mcRawDo(mc, "cas counter 16 0 1 "+cas+"\r\n"+
				value+"\r\n")
			assert.Nil(t, err)
			return resp == "STORED\r\n"
		}
		if !increment() {
			time.Sleep(100 * time.Millisecond)
			increment()
		}
	}

	mc, err := net.Dial("tcp", addr)
	assert.Nil(t, err)
	defer mc.Close()
	_, err = mcRawDo(mc, "delete counter\r\n")
	assert.Nil(t, err)

	resp, err := mcRawDo(mc, "add counter 16 0 1\r\n0\r\n")
	assert.Nil(t, err)
	assert.Equal(t, "STORED\r\n", resp)

	wg.Add(2)

	go worker(1)
	go worker(2)

	wg.Wait()

	resp, err = mcRawDo(mc, "get counter\r\n")
	assert.Nil(t, err)
	assert.Equal(t, "VALUE counter 16 1\r\n2\r\nEND\r\n", resp)
}
