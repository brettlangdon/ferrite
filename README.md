fast-cache
==========

A very fast kyoto cabinet powered memcached interface API proxy caching server.

This is not your normal caching proxy server. For starters it doesn't speak HTTP (other than to the backend),
you interact with it via a subset of the memcached commands. The main purpose is you give it a url like:
`http://api.my-domain.com/api/` and then you make memcache get calls like `get/users` or `user/12` or any other
GET only api end point. The result content does not matter at all (which means you could use this to cache HTTP get calls).

The other _very_ important thing about `fast-cache` is that when it gets a cache miss it does now wait around for a response
from the proxy server, it will send back an empty string to the client making the request and then queue up the request to
the proxy server. This means that although you will get back an empty string when you get a cache miss your response times
from this proxy server will be consistent (which can be very important for high performance applications).

## Install
### Requirements
 * Kyoto Cabinet Installed
 * libcurl Installed

```bash
git clone git://github.com/brettlangdon/fast-cache.git
cd ./fast-cache
make
# now you have a binary "./fast-cache" available, there is not `make install` yet
```

## Using
Just use your favorite memcache client
```python
import pymemcache.client
mc = pymemcache.client.Client([("127.0.0.1", 7000)])
users = mc.get("/all/users")
```

```bash
telnet 127.0.0.1 7000
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
stats
STAT connections 1
STAT requests 0
STAT hits 0
STAT misses 0
STAT hit_ratio 0.0000
STAT backlog 0
STAT bnum 1048583
STAT capcnt -1
STAT capsiz -1
STAT chksum 255
STAT count 0
STAT fmtver 5
STAT librev 13
STAT libver 16
STAT opts 0
STAT path *
STAT realtype 32
STAT recovered 0
STAT reorganized 0
STAT size 8390432
STAT type 32
END
get all/users
VALUE all/users 0 0

END
get all/users
VALUE all/users 0 2
{}
END
```
