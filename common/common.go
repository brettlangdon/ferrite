package common

import (
	"flag"
)

var HITS int32 = 0
var MISSES int32 = 0
var CONNECTIONS int32 = 0
var TTL *int = flag.Int("ttl", 3600, "the TTL in seconds for a newly cached object")
var BIND *string = flag.String("bind", "0.0.0.0:7000", "the [<address>]:<port> to bind to")
var CACHE *string = flag.String("cache", "*", "the kyoto cabinet cache path")
var VERSION string = "0.0.1"
