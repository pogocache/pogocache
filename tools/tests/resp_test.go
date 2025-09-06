package tests

import (
	"fmt"
	"sort"
	"strings"
	"testing"
	"time"

	"github.com/gomodule/redigo/redis"
	"github.com/stretchr/testify/assert"
)

func TestRESPVarious(t *testing.T) {
	conn, err := redis.Dial("tcp", ":9401")
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	reply, err := redis.String(conn.Do("FLUSH"))
	if err != nil {
		t.Fatal(err)
	}
	if reply != "OK" {
		t.Fatalf("expected OK, got %s\n", reply)
	}
	keys := make(map[string]string)
	for len(keys) < 10000 {
		keys[randString(1024)] = randString(1024)
	}
	var count int
	for key, val := range keys {
		reply, err := redis.String(conn.Do("SET", key, val))
		if err != nil {
			t.Fatal(err)
		}
		if reply != "OK" {
			t.Fatalf("expected OK, got %s\n", reply)
		}
		count++
		n, err := redis.Int(conn.Do("DBSIZE"))
		if err != nil {
			t.Fatal(err)
		}
		if n != count {
			t.Fatalf("expected %d, got %d\n", count, n)
		}
		reply, err = redis.String(conn.Do("GET", key))
		if err != nil {
			t.Fatal(err)
		}
		if reply != val {
			t.Fatalf("expected %v, got %v\n", val, reply)
		}
	}
	for key := range keys {
		reply, err := redis.Int(conn.Do("DEL", key))
		if err != nil {
			t.Fatal(err)
		}
		if reply != 1 {
			t.Fatalf("expected 1, got %d\n", reply)
		}
		count--
		n, err := redis.Int(conn.Do("DBSIZE"))
		if err != nil {
			t.Fatal(err)
		}
		if n != count {
			t.Fatalf("expected %d, got %d\n", count, n)
		}
		val, err := conn.Do("GET", key)
		if err != nil {
			t.Fatal(err)
		}
		if val != nil {
			fmt.Printf("expecte nil, got %v\n", val)
		}
	}
}

func TestRESPPostCheck(t *testing.T) {
	conn, err := redis.Dial("tcp", ":9401")
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	reply, err := redis.String(conn.Do("FLUSH"))
	if err != nil {
		t.Fatal(err)
	}
	if reply != "OK" {
		t.Fatalf("expected OK, got %s\n", reply)
	}
	keys := make(map[string]string)
	for len(keys) < 100000 {
		keys[randBytes(500)] = randBytes(500)
	}

	for key, val := range keys {
		reply, err := redis.String(conn.Do("SET", key, val))
		if err != nil {
			t.Fatal(err)
		}
		if reply != "OK" {
			t.Fatalf("expected OK, got %s\n", reply)
		}
	}

	n, err := redis.Int(conn.Do("DBSIZE"))
	if err != nil {
		t.Fatal(err)
	}
	if n != len(keys) {
		t.Fatalf("expected %d, got %d\n", len(keys), n)
	}

	for key, val := range keys {
		reply, err := redis.String(conn.Do("GET", key))
		if err != nil {
			t.Fatal(err)
		}
		if reply != val {
			t.Fatalf("expected '%s', got '%s'\n", val, reply)
		}
	}
}

func TestRESPKeyCmds(t *testing.T) {
	conn, err := redis.Dial("tcp", ":9401")
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	reply, err := redis.String(conn.Do("FLUSH"))
	if err != nil {
		t.Fatal(err)
	}
	if reply != "OK" {
		t.Fatalf("expected OK, got %s\n", reply)
	}
	keys := make(map[string]string)
	for len(keys) < 10000 {
		keys[randString(1024)] = randString(1024)
	}
	var keyarr []string
	for key := range keys {
		keyarr = append(keyarr, key)
	}
	for key, val := range keys {
		reply, err := redis.String(conn.Do("SET", key, val))
		assert.Equal(t, "OK", reply)
		assert.Equal(t, nil, err)
	}
	t.Run("SET", func(t *testing.T) {
		reply, err := redis.String(conn.Do("SET", "hello", "world"))
		assert.Equal(t, reply, "OK")
		assert.Nil(t, err)
	})
	t.Run("GET", func(t *testing.T) {
		reply, err := redis.String(conn.Do("GET", "hello"))
		assert.Equal(t, reply, "world")
		assert.Nil(t, err)
	})
	t.Run("DEL", func(t *testing.T) {
		reply, err := redis.Int(conn.Do("DEL", "hello"))
		assert.Equal(t, reply, 1)
		assert.Nil(t, err)
	})
	t.Run("MGET", func(t *testing.T) {
		var args []interface{}
		shuffle(keyarr)
		for i := range 20 {
			args = append(args, keyarr[i])
		}
		reply, err := redis.Strings(conn.Do("MGET", args...))
		assert.Nil(t, err)
		assert.Equal(t, 20, len(reply))
		for i := range reply {
			assert.Equal(t, keys[keyarr[i]], reply[i])
		}
	})
	t.Run("TTL", func(t *testing.T) {
		conn.Do("SET", "hello", "world")
		reply, err := redis.Int(conn.Do("TTL", "hello"))
		assert.Equal(t, -1, reply)
		assert.Nil(t, err)
		reply, err = redis.Int(conn.Do("TTL", "hello2"))
		assert.Equal(t, -2, reply)
		assert.Nil(t, err)
		conn.Do("SET", "hello", "world", "EX", 10)
		reply, err = redis.Int(conn.Do("TTL", "hello"))
		assert.Greater(t, reply, 0)
		assert.Nil(t, err)
		conn.Do("SET", "hello", "world", "PX", 50)
		time.Sleep(time.Millisecond * 60)
		reply, err = redis.Int(conn.Do("TTL", "hello"))
		assert.Equal(t, -2, reply)
		assert.Nil(t, err)
	})
	t.Run("PTTL", func(t *testing.T) {
		conn.Do("SET", "hello", "world")
		reply, err := redis.Int(conn.Do("PTTL", "hello"))
		assert.Equal(t, -1, reply)
		assert.Nil(t, err)
		reply, err = redis.Int(conn.Do("PTTL", "hello2"))
		assert.Equal(t, -2, reply)
		assert.Nil(t, err)
		conn.Do("SET", "hello", "world", "EX", 10)
		reply, err = redis.Int(conn.Do("PTTL", "hello"))
		assert.Greater(t, reply, 1000)
		assert.Nil(t, err)
		conn.Do("SET", "hello", "world", "PX", 50)
		time.Sleep(time.Millisecond * 60)
		reply, err = redis.Int(conn.Do("PTTL", "hello"))
		assert.Equal(t, -2, reply)
		assert.Nil(t, err)
	})
	t.Run("EXPIRE", func(t *testing.T) {
		reply, err := redis.Int(conn.Do("EXPIRE", "hello", 10))
		assert.Equal(t, 0, reply)
		assert.Nil(t, err)
		conn.Do("SET", "hello", "world")
		reply, err = redis.Int(conn.Do("EXPIRE", "hello", 10))
		assert.Equal(t, 1, reply)
		assert.Nil(t, err)
		reply, err = redis.Int(conn.Do("TTL", "hello"))
		assert.Greater(t, reply, 0)
		assert.Nil(t, err)
		conn.Do("DEL", "hello")
	})
	t.Run("DBSIZE", func(t *testing.T) {
		reply, err := redis.Int(conn.Do("DBSIZE"))
		assert.Equal(t, len(keys), reply)
		assert.Nil(t, err)
	})
	t.Run("EXISTS", func(t *testing.T) {
		reply, err := redis.Int(conn.Do("EXISTS", "hello"))
		assert.Equal(t, 0, reply)
		assert.Nil(t, err)
		shuffle(keyarr)
		var args []interface{}
		for i := range 20 {
			args = append(args, keyarr[i])
		}
		reply, err = redis.Int(conn.Do("EXISTS", args...))
		assert.Equal(t, 20, reply)
		assert.Nil(t, err)
	})
	t.Run("KEYS", func(t *testing.T) {
		reply, err := redis.Strings(conn.Do("KEYS", "*"))
		assert.Equal(t, len(keys), len(reply))
		assert.Nil(t, err)
		sort.Strings(reply)

		for i := 0; i < len(reply); i++ {
			if i > 0 {
				assert.NotEqual(t, reply[i], reply[i-1])
			}
			_, ok := keys[reply[i]]
			assert.True(t, ok)
		}

		subkeys := make(map[string]string)
		for key, val := range keys {
			if strings.HasPrefix(key, "a") {
				subkeys[key] = val
			}
		}

		reply, err = redis.Strings(conn.Do("KEYS", "a*"))
		assert.Equal(t, len(subkeys), len(reply))
		assert.Nil(t, err)
		sort.Strings(reply)

		for i := 0; i < len(reply); i++ {
			if i > 0 {
				assert.NotEqual(t, reply[i], reply[i-1])
			}
			_, ok := subkeys[reply[i]]
			assert.True(t, ok)
		}

		subkeys = make(map[string]string)
		for key, val := range keys {
			if strings.HasSuffix(key, "a") {
				subkeys[key] = val
			}
		}
		reply, err = redis.Strings(conn.Do("KEYS", "*a"))
		assert.Equal(t, len(subkeys), len(reply))
		assert.Nil(t, err)
		sort.Strings(reply)

		for i := 0; i < len(reply); i++ {
			if i > 0 {
				assert.NotEqual(t, reply[i], reply[i-1])
			}
			_, ok := subkeys[reply[i]]
			assert.True(t, ok)
		}
	})
	t.Run("INCR", func(t *testing.T) {
		reply, err := redis.Int(conn.Do("INCR", "hello"))
		assert.Equal(t, 1, reply)
		assert.Nil(t, err)
		reply, err = redis.Int(conn.Do("INCR", "hello"))
		assert.Equal(t, 2, reply)
		assert.Nil(t, err)
		conn.Do("SET", "hello", "world")
		reply, err = redis.Int(conn.Do("INCR", "hello"))
		assert.Equal(t, 0, reply)
		assert.Errorf(t, err, "ERR value is not an integer or out of range")
		conn.Do("SET", "hello", "9223372036854775806")
		reply, err = redis.Int(conn.Do("INCR", "hello"))
		assert.Equal(t, 9223372036854775807, reply)
		assert.Nil(t, err)
		reply, err = redis.Int(conn.Do("INCR", "hello"))
		assert.Equal(t, 0, reply)
		assert.Errorf(t, err, "ERR value is not an integer or out of range")
		conn.Do("SET", "hello", "-9223372036854775809")
		reply, err = redis.Int(conn.Do("INCR", "hello"))
		assert.Equal(t, 0, reply)
		assert.Errorf(t, err, "ERR value is not an integer or out of range")
		conn.Do("SET", "hello", "-9223372036854775808")
		reply, err = redis.Int(conn.Do("INCR", "hello"))
		assert.Equal(t, -9223372036854775807, reply)
		assert.Nil(t, err)
		conn.Do("DEL", "hello")
	})
	t.Run("DECR", func(t *testing.T) {
		reply, err := redis.Int(conn.Do("DECR", "hello"))
		assert.Equal(t, -1, reply)
		assert.Nil(t, err)
		reply, err = redis.Int(conn.Do("DECR", "hello"))
		assert.Equal(t, -2, reply)
		assert.Nil(t, err)
		conn.Do("SET", "hello", "world")
		reply, err = redis.Int(conn.Do("DECR", "hello"))
		assert.Equal(t, 0, reply)
		assert.Errorf(t, err, "ERR value is not an integer or out of range")
		conn.Do("SET", "hello", "-9223372036854775807")
		reply, err = redis.Int(conn.Do("DECR", "hello"))
		assert.Equal(t, -9223372036854775808, reply)
		assert.Nil(t, err)
		reply, err = redis.Int(conn.Do("DECR", "hello"))
		assert.Equal(t, 0, reply)
		assert.Errorf(t, err, "ERR value is not an integer or out of range")
		conn.Do("SET", "hello", "-9223372036854775809")
		reply, err = redis.Int(conn.Do("INCR", "hello"))
		assert.Equal(t, 0, reply)
		assert.Errorf(t, err, "ERR value is not an integer or out of range")
		conn.Do("SET", "hello", "9223372036854775807")
		reply, err = redis.Int(conn.Do("DECR", "hello"))
		assert.Equal(t, 9223372036854775806, reply)
		assert.Nil(t, err)
		conn.Do("DEL", "hello")
	})
	t.Run("UINCR", func(t *testing.T) {
		reply, err := redis.String(conn.Do("UINCR", "hello"))
		assert.Equal(t, "1", reply)
		assert.Nil(t, err)
		conn.Do("SET", "hello", "18446744073709551614")
		reply, err = redis.String(conn.Do("UINCR", "hello"))
		assert.Equal(t, "18446744073709551615", reply)
		assert.Nil(t, err)
		reply, err = redis.String(conn.Do("UINCR", "hello"))
		assert.Equal(t, "", reply)
		assert.Errorf(t, err, "ERR value is not an integer or out of range")
		conn.Do("DEL", "hello")
	})
	t.Run("UDECR", func(t *testing.T) {
		reply, err := redis.String(conn.Do("UDECR", "hello"))
		assert.Equal(t, "", reply)
		assert.Errorf(t, err, "ERR value is not an integer or out of range")
		conn.Do("SET", "hello", "18446744073709551615")
		reply, err = redis.String(conn.Do("UDECR", "hello"))
		assert.Equal(t, "18446744073709551614", reply)
		assert.Nil(t, err)
		reply, err = redis.String(conn.Do("UDECR", "hello"))
		assert.Equal(t, "18446744073709551613", reply)
		assert.Nil(t, err)
		conn.Do("DEL", "hello")
	})
	t.Run("INCRBY", func(t *testing.T) {
		reply, err := redis.Int(conn.Do("INCRBY", "hello", 10))
		assert.Equal(t, 10, reply)
		assert.Nil(t, err)
		reply, err = redis.Int(conn.Do("INCRBY", "hello", 25))
		assert.Equal(t, 35, reply)
		assert.Nil(t, err)
		conn.Do("DEL", "hello")
	})
	t.Run("DECRBY", func(t *testing.T) {
		conn.Do("SET", "hello", "9223372036854775807")
		reply, err := redis.Int(conn.Do("DECRBY", "hello", 10))
		assert.Equal(t, 9223372036854775797, reply)
		assert.Nil(t, err)
		reply, err = redis.Int(conn.Do("DECRBY", "hello", 25))
		assert.Equal(t, 9223372036854775772, reply)
		assert.Nil(t, err)
		conn.Do("DEL", "hello")
	})
	t.Run("UINCRBY", func(t *testing.T) {
		reply, err := redis.String(conn.Do("UINCRBY", "hello", 10))
		assert.Equal(t, "10", reply)
		assert.Nil(t, err)
		reply, err = redis.String(conn.Do("UINCRBY", "hello", 25))
		assert.Equal(t, "35", reply)
		assert.Nil(t, err)
		conn.Do("DEL", "hello")
	})
	t.Run("UDECRBY", func(t *testing.T) {
		conn.Do("SET", "hello", "9223372036854775807")
		reply, err := redis.String(conn.Do("UDECRBY", "hello", 10))
		assert.Equal(t, "9223372036854775797", reply)
		assert.Nil(t, err)
		reply, err = redis.String(conn.Do("UDECRBY", "hello", 25))
		assert.Equal(t, "9223372036854775772", reply)
		assert.Nil(t, err)
		conn.Do("DEL", "hello")
	})
	t.Run("APPEND", func(t *testing.T) {
		conn.Do("SET", "hello", "A")
		n, err := redis.Int(conn.Do("APPEND", "hello", "BC"))
		assert.Equal(t, 3, n)
		assert.Nil(t, err)
		reply, err := redis.String(conn.Do("GET", "hello"))
		assert.Equal(t, "ABC", reply)
		assert.Nil(t, err)
		conn.Do("DEL", "hello")
	})
	t.Run("PREPEND", func(t *testing.T) {
		conn.Do("SET", "hello", "A")
		n, err := redis.Int(conn.Do("PREPEND", "hello", "BC"))
		assert.Equal(t, 3, n)
		assert.Nil(t, err)
		reply, err := redis.String(conn.Do("GET", "hello"))
		assert.Equal(t, "BCA", reply)
		assert.Nil(t, err)
		conn.Do("DEL", "hello")
	})
}
