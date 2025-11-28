package tests

import (
	"context"
	"strconv"
	"testing"

	"github.com/jackc/pgx/v5"
)

func TestPostgres(t *testing.T) {
	url := "postgres://127.0.0.1:9401"
	conn, err := pgx.Connect(context.Background(), url)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close(context.Background())

	tag, err := conn.Exec(context.Background(), "FLUSH")
	if err != nil {
		t.Fatal(err)
	}
	if tag.String() != "FLUSH SYNC" {
		t.Fatalf("expected 'FLUSH SYNC' got '%s'\n", tag)
	}

	keys := make(map[string]string)
	for len(keys) < 10000 {
		key := randString(1024)
		if len(key) > 0 {
			keys[key] = randString(1024)
		}
	}
	var count int
	for key, val := range keys {
		tag, err := conn.Exec(context.Background(), "SET $1 $2", key, val)
		if err != nil {
			t.Fatal(err)
		}
		if tag.String() != "SET 1" {
			t.Fatalf("expected 'SET 1' got '%s'\n", tag)
		}
		count++
		var str string
		err = conn.QueryRow(context.Background(), "DBSIZE").Scan(&str)
		if err != nil {
			t.Fatal(err)
		}
		n, _ := strconv.Atoi(str)
		if n != count {
			t.Fatalf("expected %d, got %d\n", count, n)
		}
		var val2 string
		err = conn.QueryRow(context.Background(), "GET $1", key).Scan(&val2)
		if err != nil {
			t.Fatal(err)
		}
		if val2 != val {
			t.Fatalf("expected %s, got %s\n", val, val2)
		}
	}

	for key := range keys {
		tag, err := conn.Exec(context.Background(), "DEL $1", key)
		if err != nil {
			t.Fatal(err)
		}
		if tag.String() != "DEL 1" {
			t.Fatalf("expected 'DEL 1' got '%s'\n", tag)
		}
		count--
		var str string
		err = conn.QueryRow(context.Background(), "DBSIZE").Scan(&str)
		if err != nil {
			t.Fatal(err)
		}
		n, _ := strconv.Atoi(str)
		if n != count {
			t.Fatalf("expected %d, got %d\n", count, n)
		}
		var val2 string
		err = conn.QueryRow(context.Background(), "GET $1", key).Scan(&val2)
		if err != pgx.ErrNoRows {
			t.Fatalf("exepected %v got %v", pgx.ErrNoRows, err)
		}
	}

}
