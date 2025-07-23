<p align="center">
<img alt="Pogocache" src=".github/images/logo.png" width="600">
</p>

Pogocache is fast caching software built from scratch with a focus on low latency and cpu efficency.

**Faster**: Pogocache is faster than Memcache, Valkey, Redis, Dragonfly, and Garnet.
It has the lowest latency per request, providing the quickest response times.
It's optimized to scale from one to many cores, giving you the best single-threaded and multithreaded performance.

**Cheaper**: Pogocache uses the fewest cpu cycles per request; minimizing server load, energy usage, and the overall cost to operate.

**Easier**: Pogocache runs as a server-based program.
It supports Memcache, Valkey/Redis, HTTP, and Postgres wire protocols,
allowing for the use of system tools such as curl and psql, and numerous client libraries that are available for most programming languages.

**Embeddable**: Optionally instead of running Pogocache as a server-based program, 
the self-contained pogocache.c file can be compiled into existing software,
bypassing the network and directly accessing the cache programmatically.
Running embedded provides raw speed, with over 100M ops per second.

---

<!--
Cache       Throughput  Latency     CPU Cycles
----------------------------------------------
redis           909837    447μs          11799
valkey         1331683    263μs          15751
dragonfly      1407584    255μs          14384
garnet         1548050    231μs          13396
memcache       2601306    191μs          13400
pogocache      3145960    111μs           6968
-->

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="/.github/images/graphs-dark.png">
  <source media="(prefers-color-scheme: light)" srcset="/.github/images/graphs-light.png">
  <img alt="Graphs" src="/.github/images/graphs-light.png">
</picture>

The above benchmarks measure the performance of Pogocache against other caching
software running 8 threads on an AWS c8g.8xlarge.  
Visit https://github.com/tidwall/cache-benchmarks to see more benchmarks.

# Table of contents

- [Gettng started](#getting-started)
    - [Building](#building)
    - [Running](#running)
    - [Connecting](#connecting)
- [Wire protocols and commands](#wire-protocols-and-commands)
    - [HTTP](#http)
    - [Memcache](#memcache)
    - [RESP (Valkey/Redis)](#resp-valkeyredis)
    - [Postgres](#postgres)
- [Security](#security)
    - [TLS/HTTPS](#tlshttps)
    - [Auth password](#auth-password)
- [Embeddable](#embeddable)
- [Design details](#design-details)
- [Roadmap and status](#roadmap-and-status)
- [License](#license)
- [Support](#support)

## Getting started

Pogocache is designed and tested on 64-bit Linux and MacOS.

### Building

```
make
```

This will build the `pogocache` program

### Running

```
./pogocache
```

This will start the server on localhost (127.0.0.1) port 9401.
To allow connections from other machines, bind the listener to an accessible
host address.

```
./pogocache -h 172.30.2.84
```

## CMake (alternative build)

```sh
$ cmake -S . -B build
$ cmake --build build
$ build/bin/pogocache_cli --version
pogocache 1.0.0-1-g9e47ab5
```

<details>
<summary>

**See all Pogocache program options**

</summary>

The help menu displayed on a Linux server with 32 CPUs. 

```
./pogocache --help
```

```
Usage: pogocache [options]

Basic options:
  -h hostname            listening host                 (default: 127.0.0.1)
  -p port                listening port                 (default: 9401)
  -s socket              unix socket file               (default: none)
  -v,-vv,-vvv            verbose logging level

Additional options:
  --threads count        number of threads              (default: 32)
  --maxmemory value      set max memory usage           (default: 80%)
  --evict yes/no         evict keys at maxmemory        (default: yes)
  --persist path         persistence file               (default: none)
  --maxconns conns       maximum connections            (default: 1024)

Security options:
  --auth passwd          auth token or password         (default: none)
  --tlsport port         enable tls on port             (default: none)
  --tlscert certfile     tls cert file                  (default: none)
  --tlskey keyfile       tls key file                   (default: none)
  --tlscacert cacertfile tls ca-cert file               (default: none)

Advanced options:
  --shards count         number of shards               (default: 4096)
  --backlog count        accept backlog                 (default: 1024)
  --queuesize count      event queuesize size           (default: 128)
  --reuseport yes/no     reuseport for tcp              (default: no)
  --tcpnodelay yes/no    disable nagles algo            (default: yes)
  --quickack yes/no      use quickack (linux)           (default: no)
  --uring yes/no         use uring (linux)              (default: yes)
  --loadfactor percent   hashmap load factor            (default: 75)
  --keysixpack yes/no    sixpack compress keys          (default: yes)
  --cas yes/no           use compare and store          (default: no)
```

</details>

### Connecting

A variety of tools may be used to connect to Pogocache.
This includes [valkey-cli](https://github.com/valkey-io/valkey), [redis-cli](https://github.com/redis/redis), [psql](https://www.postgresql.org/docs/current/app-psql.html), and [curl](https://curl.se/docs/manpage.html).
Telnet is also available, which uses the [Memcache](#memcache) interface.

Here's an example using curl

```sh
$ curl -X PUT -d "my value" http://localhost:9401/mykey
Stored

$ curl http://localhost:9401/mykey
my value

$ curl -X DELETE http://localhost:9401/mykey
Deleted
```

Here's an example using a RESP (Valkey/Redis) client.

```sh
$ valkey-cli -p 9401
> SET mykey value
OK
> GET mykey
"my value"
> DEL mykey
(integer) 1
```

Here's an example using psql, the Postgres command line tool.

```sh
psql -h localhost -p 9401
=> SET mykey 'my value';
=> GET mykey;
=> DEL mykey;
```

## Wire protocols and commands

Pogocache supports the following wire protocols.

- [HTTP](#http)
- [Memcache](#memcache)
- [Postgres](#postgres)
- [RESP (Valkey/Redis)](#resp-valkeyredis)

### HTTP

Pogocache uses HTTP methods PUT, GET, DELETE to store, retrieve, and delete
entries.

#### Store entry

```
PUT /:key
```

##### Params

| Param | Required | Description |
| ----- | -------- | ----------- |
| key   |      yes | Key of entry |
| ttl   |       no | Time to live in seconds |
| nx    |       no | Only store if it does not already exist. |
| xx    |       no | Only store if it already exists. |
| auth  |       no | Auth password |

##### Returns

- `200 OK` with the response "Stored"

##### Examples

```sh
curl -X PUT -d "my value" "http://localhost:9401/mykey"        # store entry
curl -X PUT -d "my value" "http://localhost:9401/mykey?ttl=15" # store with 15 second ttl
```

#### Get entry

```
GET /:key
```

##### Params

| Param | Required | Description |
| ----- | -------- | ----------- |
| key   |      yes | Key of entry |
| auth  |       no | Auth password |

##### Returns

- `200 OK` and the value in the body
- `404 Not Found` and the value "Not Found"

#### Delete entry

```sh
curl -X PUT -d "my value" "http://localhost:9401/mykey"        # store entry
```

```
DELETE /:key
```

##### Returns

- `200 OK` with the respone "Deleted"
- `404 Not Found` and the value "Not Found"

##### Params

| Param | Required | Description |
| ----- | -------- | ----------- |
| key   |      yes | Key of entry |
| auth  |       no | Auth password |


### Memcache

Pogocache supports the commands from the [Memcache text protocol](https://docs.memcached.org/protocols/basic/), including
`set`, `add`, `replace`, `append`, `prepend`, `cas`
`get`, `gets`, `delete`, `incr/decr`, `flush_all`


### RESP (Valkey/Redis)

Pogocache has the following RESP commands, which can be used in your favorite
Valkey/Redis command line tool or client library.

[SET](#resp-set), [GET](#resp-get), [DEL](#resp-del), [MGET](#resp-mget), [MGETS](#resp-mgets), [TTL](#resp-ttl), [PTTL](#resp-pttl), [EXPIRE](#resp-expire), [DBSIZE](#resp-dbsize), [QUIT](#resp-quit), [ECHO](#resp-echo), [EXISTS](#resp-exists), [FLUSH](#resp-flush), [PURGE](#resp-purge), [SWEEP](#resp-sweep), [KEYS](#resp-keys), [PING](#resp-ping), [INCRBY](#resp-incrby), [DECRBY](#resp-decrby), [INCR](#resp-incr), [DECR](#resp-decr), [UINCRBY](#resp-uincrby), [UDECRBY](#resp-udecrby), [UINCR](#resp-uincr), [UDECR](#resp-udecr), [APPEND](#resp-append), [PREPEND](#resp-prepend), [AUTH](#resp-auth), [SAVE](#resp-save), [LOAD](#resp-load)

<table>
<tr><td>
  <a name="resp-set"></a>
  <b>SET key value [NX | XX] [GET] [EX seconds | PX milliseconds | EXAT unix-time-seconds | PXAT unix-time-milliseconds | KEEPTTL] [FLAGS flags] [CAS cas]</b><br><br>
  Store a value for key.
</td></tr>
<tr><td>
<a name="resp-get"></a>
  <b>GET key</b><br><br>
  Get a value for key.
</td></tr>
<tr><td>
  <a name="resp-del"></a>
  <b>DEL key</b><br><br>
  Delete a value for key.
</td></tr>
<tr><td>
  <a name="resp-mget"></a>
  <b>MGET key [key ...]</b><br><br>
  Get values for multiple keys.
</td></tr>
<tr><td>
  <a name="resp-mgets"></a>
  <b>MGETS key [key ...]</b><br><br>
  Get value, flag, and CAS for multiple keys
  <br><br>
  To get a non-zero cas number, the CAS feature in Pogocache must be active using `--cas=yes` program flag.
</td></tr>
<tr><td>
  <a name="resp-ttl"></a>
  <b>TTL key</b><br><br>
  Get the remaining seconds for time to live for a key.
</td></tr>
<tr><td>
  <a name="resp-pttl"></a>
  <b>PTTL key</b><br><br>
  Get the remaining milliseconds for time to live for a key.
</td></tr>
<tr><td>
  <a name="resp-expire"></a>
  <b>EXPIRE key seconds</b><br><br>
  Set or reset a time to live, as seconds, on a key.
</td></tr>
<tr><td>
  <a name="resp-dbsize"></a>
  <b>DBSIZE</b><br><br>
  Get the total number of keys in cache.
</td></tr>
<tr><td>
  <a name="resp-quit"></a>
  <b>QUIT</b><br><br>
  Quit the current client connection
</td></tr>
<tr><td>
  <a name="resp-echo"></a>
  <b>ECHO message</b><br><br>
  Send the server a message, which is sent back.
</td></tr>
<tr><td>
  <a name="resp-ping"></a>
  <b>PING [message]</b><br><br>
  Ping the server with an optional message.
</td></tr>
<tr><td>
  <a name="resp-exists"></a>
  <b>EXISTS key [key ...]</b><br><br>
  Checks if one or more keys exist in the cache.
</td></tr>
<tr><td>
  <a name="resp-flush"></a>
  <b>FLUSH [SYNC|ASYNC] [DELAY <seconds>]</b><br><br>
  Clears the cache.
</td></tr>
<tr><td>
  <a name="resp-sweep"></a>
  <b>SWEEP [ASYNC]</b><br><br>
  Removes all expired entries from the cache.
</td></tr>
<tr><td>
  <a name="resp-purge"></a>
  <b>PURGE [ASYNC]</b><br><br>
  Release unused memory back to the operating system. 
</td></tr>
<tr><td>
  <a name="resp-keys"></a>
  <b>KEYS pattern</b><br><br>
  Get keys matching pattern. 
  <br><br>
  Supports a very simple pattern matcher where '*' matches on any number characters and '?' matches on any one character.
</td></tr>
<tr><td>
  <a name="resp-incrby"></a>
  <b>INCRBY key delta</b><br><br>
  Increment a 64-bit signed integer by delta.
</td></tr>
<tr><td>
  <a name="resp-decrby"></a>
  <b>DECRBY key delta</b><br><br>
  Decrement a 64-bit signed integer by delta.
</td></tr>
<tr><td>
  <a name="resp-incr"></a>
  <b>INCR key</b><br><br>
  Increment a 64-bit signed integer by one.
</td></tr>
<tr><td>
  <a name="resp-decr"></a>
  <b>DECR key</b><br><br>
  Decrement a 64-bit signed integer by one.
</td></tr>
<tr><td>
  <a name="resp-uincrby"></a>
  <b>UINCRBY key delta</b><br><br>
  Increment a 64-bit unsigned integer by delta.
</td></tr>
<tr><td>
  <a name="resp-udecrby"></a>
  <b>UDECRBY key delta</b><br><br>
  Decrement a 64-bit unsigned integer by delta.
</td></tr>
<tr><td>
  <a name="resp-uincr"></a>
  <b>UINCR key</b><br><br>
  Increment a 64-bit unsigned integer by delta.
</td></tr>
<tr><td>
  <a name="resp-udecr"></a>
  <b>UDECR key</b><br><br>
  Decrement a 64-bit unsigned integer by delta.
</td></tr>
<tr><td>
  <a name="resp-append"></a>
  <b>APPEND key value</b><br><br>
  Concatenate value at the end of the current string.
</td></tr>
<tr><td>
  <a name="resp-prepend"></a>
  <b>PREPEND key value</b><br><br>
  Concatenate value at the start of the current string.
</td></tr>
<tr><td>
  <a name="resp-auth"></a>
  <b>AUTH password</b><br><br>
  Authorize the current connection using the same password provided to the Pogocache `--auth` flag.
</td></tr>
<tr><td>
  <a name="resp-save"></a>
  <b>SAVE [TO path] [FAST]</b><br><br>
  Save a copy of the cache to file provided at path. If a path is not provided then the cache is saved to the file assigned to the Pogocache `--persist` flag.
  <br><br>
  Use FAST to make the saving process use all machine cores, making the operation
  finish quicker, but may slow down other concurrent connections when the cache is very large.
</td></tr>
<tr><td>
  <a name="resp-load"></a>
  <b>LOAD [FROM path] [FAST]</b><br><br>
  Load a copy of the cache from file provided at path. If a path is not provided then the cache is saved to the file assigned to the Pogocache `--persist` flag.
  <br><br>
  Use FAST to make the saving process use all machine cores, making the operation
  finish quicker, but may slow down other concurrent connections when the cache is very large.
</td></tr>
</table>

### Postgres

The postgres commands are the same as the RESP above.

Here's an example in Python using the [psycopg](https://www.psycopg.org) client.

```py
import psycopg

# Connect to Pogocache
conn = psycopg.connect("host=localhost port=9401")

# Insert some values
conn.execute("SET first Tom")
conn.execute("SET last Anderson")
conn.execute("SET age 37")
conn.execute("SET city %s", ["Phoenix"])

# Get the values back
print(conn.execute("GET first").fetchone()[0])
print(conn.execute("GET last").fetchone()[0])
print(conn.execute("GET age").fetchone()[0])
print(conn.execute("GET %s", ["city"]).fetchone()[0])

# Loop over multiple values
for row in conn.execute("MGET first last city"):
    print(row[0], row[1])

conn.close()

# Output:
# Tom
# Anderson
# 37
# first Tom
# last Anderson
# city Phoenix
```

Using psql

```sh
psql -h localhost -p 9401
=> SET first Tom;
=> SET last Anderson;
=> SET age 37;
```

#### Binary data with Postgres client

The default Postgres output format in Pogocache is `::text`.
This is generally the perferred format and allows for the most seamless text
translations between client and server.

But occasionally storing binary is needed, such as with image, PDFs, or other special
data formats.
While Pogocache can transparently handle both text and binary, some client libraries
may have problems convering text <-> binary due to strict type rules.

Pogocache provides the `::text` and `::bytea` command directives that will
switch the output format for the client session.

While in `::bytea` mode, it becomes the programmers responsibility to convert
between binary and text.

```py
import psycopg

conn = psycopg.connect("host=localhost port=9401")

conn.execute("SET name %s", ["Tom"])
conn.execute("SET binary %s", [b'\x00\x01\x02hello\xff'])

conn.execute("::bytea")
print(conn.execute("GET name").fetchone()[0])
print(conn.execute("GET binary").fetchone()[0])

conn.execute("::text")
print(conn.execute("GET name").fetchone()[0])

# Output:
# b'Tom'
# b'\x00\x01\x02hello\xff'
# Tom
```

#### Postgres parameterize queries

Parameterize queries are allowed with Pogocache. The example above shows how
the `psycopg` uses the `%s` as a placeholder for a parameter. Other client may
use the standard Postgres placeholders, such as `$1`, `$2`, `$3`.

For example, with the popular [jackc/pgx](https://github.com/jackc/pgx) client for Go:

```go
package main

import (
	"context"
	"fmt"
	"os"

	"github.com/jackc/pgx/v5"
)

func main() {
	// Connect to Pogocache
	url := "postgres://127.0.0.1:9401"
	conn, err := pgx.Connect(context.Background(), url)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Unable to connect to cache: %v\n", err)
		os.Exit(1)
	}
	defer conn.Close(context.Background())

	conn.Exec(context.Background(), "SET $1 $2", "first", "Tom")
	row := conn.QueryRow(context.Background(), "GET $1", "first")

	var value string
	row.Scan(&value)
	println(value)

	userID := "1287"
	conn.Exec(context.Background(), "SET user:$1:first $2", userID, "Tom")
	conn.Exec(context.Background(), "SET user:$1:last $2", userID, "Anderson")
	conn.Exec(context.Background(), "SET user:$1:age $2", userID, "37")

	var first, last, age string

	conn.QueryRow(context.Background(), "GET user:$1:first", userID).Scan(&first)
	conn.QueryRow(context.Background(), "GET user:$1:last", userID).Scan(&last)
	conn.QueryRow(context.Background(), "GET user:$1:age", userID).Scan(&age)

	println(first)
	println(last)
	println(age)
}

// Output:
// Tom
// Tom
// Anderson
// 37
```

Any string or keyword can be parameterized. 

## Security

- [TLS/HTTPS](#tlshttps)
- [Auth password](#auth-password)

### TLS/HTTPS

To use TLS, you need to start Pogocache with TLS-specific program flags.

#### Example TLS certificate generation

You’ll need:

- CA certificate: ca.crt
- Server certificate: pogocache.crt
- Server private key: pogocache.key

```s
# Generate CA key and cert
openssl req -x509 -newkey rsa:4096 -sha256 -days 365 -nodes \
  -keyout ca.key -out ca.crt -subj "/CN=My Pogocache CA"

# Generate server key and CSR
openssl req -newkey rsa:4096 -nodes -keyout pogocache.key -out pogocache.csr -subj "/CN=localhost"

# Sign server cert with CA
openssl x509 -req -sha256 -days 365 -in pogocache.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out pogocache.crt
```

#### Start Pogocache with TLS

```sh
pogocache -p 0             \
   --tlsport 9401          \
   --tlscert pogocache.crt \
   --tlskey pogocache.key  \
   --tlscacert ca.crt
```

The `-p 0` disables the plain-text port.

#### Connect to Pogocache with TLS

##### Using valkey/redis cli

```sh
valkey-cli --tls         \
    --cert pogocache.crt \
    --key pogocache.key  \
    --cacert ca.crt      \
    -h localhost -p 9401
```

##### Using curl

```sh
curl "https://localhost:9401" \
  --cert pogocache.crt        \
  --key pogocache.key         \
  --cacert ca.crt
```

### Auth password

An optional auth password may be provide to Pogocache through the `--auth` program flag.
When an auth password is being used on the server, that password must be provided with all client request. 

```sh
pogocache --auth mypass
```

Now all connection must supply the auth password

```sh
valkey-cli -p 9401 -a mypass
```

```sh
curl -H "Authorization: Bearer mypass" "http://localhost:9401/mykey" # Use header
curl "http://localhost:9401/mykey?auth=mypass"                       # Use querystring
```

## Embeddable

The pogocache.c file in the src directory is a standalone library that can be compiled directly into an existing project that supports C.
This makes available an interface to the primary caching operations provided by Pogocache.

For example:

```c
// prog.c
#include <stdio.h>
#include "pogocache.h"

static void load_callback(int shard, int64_t time, const void *key,
    size_t keylen, const void *value, size_t valuelen, int64_t expires, 
    uint32_t flags, uint64_t cas, struct pogocache_update **update, 
    void *udata)
{
    printf("%.*s\n", (int)valuelen, (char*)value);
}

int main(void) {
    // Create a Pogocache instance
    struct pogocache *cache = pogocache_new(0);

    // Store some values
    pogocache_store(cache, "user:1391:name", 14, "Tom", 3, 0);
    pogocache_store(cache, "user:1391:last", 14, "Anderson", 8, 0);
    pogocache_store(cache, "user:1391:age", 13, "37", 2, 0);

    // Read the values back
    struct pogocache_load_opts lopts = { .entry = load_callback };
    pogocache_load(cache, "user:1391:name", 14, &lopts);
    pogocache_load(cache, "user:1391:last", 14, &lopts);
    pogocache_load(cache, "user:1391:age", 13, &lopts);
    return 0;
}
// Tom
// Anderson
// 37
```

Compile and run:

```sh
cc prog.c pogocache.c
./a.out
```

See the [src/pogocache.h](src/pogocache.h) and [src/pogocache.c](src/pogocache.c) for more information.

## Design details

### Overview

Pogocache is fast caching software.
It's goal is to provide low latency and cpu efficency.
It's written in the C programing language and runs on MacOS and Linux.

### Hashmap and shards

Pogocache stores entries (key value pairs) in a sharded hashmap. 
That sharded hashmap has a high fanout, often 4096 shards.
The number of shards is automatically configured at startup and can be changed by the user.

Each shard has its own independent hashmap. That hashmap uses [open addressing](https://en.wikipedia.org/wiki/Hash_table#Open_addressing) with [Robin hood hashing](https://en.wikipedia.org/wiki/Hash_table#Robin_Hood_hashing).
The concept for this design originated from the [tidwall/shardmap](https://github.com/tidwall/shardmap) project. 

The hashmap buckets are 10-bytes. One byte for the dib (distance to bucket), three bytes for the hash, and six bytes for the entry pointer.

Each entry is a single allocation on the heap. 
This allocation contains a one byte header and a series of fields.
These fields always include one key and one value. 
There may also be various optional fields such as expiry, cas, and flags.
The key is stored using the bespoke sixpack compression, which is just a minor optimization for storing keys using six bits per byte rather than eight. 

When inserting or retrieving an entry, the entry's key is hashed into a 64-bit number.
From that 64-bit hash, the high 32-bits are used to determine the shard and the low 32-bits are used for the per-shard hashmap.
The hash function used is [tidwall/th64](https://github.com/tidwall/th64).
Each hash call uses a seed that is crypto-randomly generated at the start of the Pogocache program.

When operating on a shard, that shard is locked for the duration of the operation using a lightweight spinlock. 

### Networking and threads

At startup Pogocache determines the number threads to use for the life of the program.
This number is based on the core count on the machine. 
At which point the network is initialized.

For each thread, an event queue (epoll or kqueue) is instantiated.
All socket connections are bound to one of the queues by round-robin selection.
Each queue waits on socket read events.
The concept for this model originated from the [tidwall/evio](https://github.com/tidwall/evio) project. 

For linux, when available, [io_uring](https://en.wikipedia.org/wiki/Io_uring) may be used for read and write batching. This is a minor optimization which can be turned off by the user.

### Expiration and eviction

All entries may have an optional expiry value. 
Upon expiration that entry will no longer be available and will be evicted from the shard.

The other way an entry may be evicted is when the program is low on memory.
When memory is low the insert operation will automatically choose to evict some older entry, using the [2-random algorithm](https://danluu.com/2choices-eviction/).

Low memory evictions free up memory immediately to make room for new entries.
Expiration evictions, on the other hand, will not free the memory until the 
entry's container bucket is accessed, or until the sweep operation is called.

## Roadmap and status

Pogocache's initial goal was to create the fastest, most efficent cache.

Next steps are:

- Build domain specific integrations for web, sql, prompts, and geo.
- Add direct access over shared memory and IPC.
- Provide enterprise tooling such as distributed routing and failovers.

## License

Pogocache is free & open source software released under the terms of the AGPL license. The Pogocache source code was written and copyrighted by Polypoint Labs, LLC. 

For alternative licensing options, please contact us at licensing@polypointlabs.com.

## Support

If you decided to use Pogocache for commercial purposes, please consider purchasing support.
Contact us at support@polypointlabs.com for more information.
