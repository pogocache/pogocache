package tests

import (
	"testing"
)

func TestHTTP(t *testing.T) {
	keys := make(map[string]string)
	for len(keys) < 1000 {
		key := randString(128)
		if key != "" {
			keys[key] = randString(1024)
		}
	}
	for key, val := range keys {
		resp, err := httpDo(nil, "PUT", "/"+key, val)
		if err != nil {
			t.Fatal(err)
		}
		if resp != "Stored\r\n" {
			t.Fatalf("expected 'Stored\\r\\n', got '%s'", resp)
		}
		resp, err = httpDo(nil, "GET", "/"+key, "")
		if err != nil {
			t.Fatal(err)
		}
		if resp != val {
			t.Fatalf("expected '%s', got '%s'", val, resp)
		}
		resp, err = httpDo(nil, "DELETE", "/"+key, "")
		if err != nil {
			t.Fatal(err)
		}
		if resp != "Deleted\r\n" {
			t.Fatalf("expected 'Deleted\r\n', got '%s'", resp)
		}
		resp, err = httpDo(nil, "GET", "/"+key, "")
		if err != nil {
			t.Fatal(err)
		}
		if resp != "Not Found\r\n" {
			t.Fatalf("expected 'Not Found\r\n', got '%s'", resp)
		}
	}

}
