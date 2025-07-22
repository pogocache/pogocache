package tests

import (
	"testing"

	"github.com/bradfitz/gomemcache/memcache"
)

func TestMemcache(t *testing.T) {
	// mc := memcache.New("127.0.0.1:11211")
	mc := memcache.New("127.0.0.1:9401")
	if err := mc.FlushAll(); err != nil {
		t.Fatal(err)
	}
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
