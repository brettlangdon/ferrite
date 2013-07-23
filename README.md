ferrite
==========

A very fast kyoto cabinet powered memcached interface API proxy caching server.

This is not your normal caching proxy server. For starters it doesn't speak HTTP (other than to the backend),
you interact with it via a subset of the memcached commands. The main purpose is you give it a url like:
`http://api.my-domain.com/api/` and then you make memcache get calls like `get/users` or `user/12` or any other
GET only api end point. The result content does not matter at all (which means you could use this to cache HTTP get calls).

The other _very_ important thing about `ferrite` is that when it gets a cache miss it does now wait around for a response
from the proxy server, it will send back an empty string to the client making the request and then queue up the request to
the proxy server. This means that although you will get back an empty string when you get a cache miss your response times
from this proxy server will be consistent (which can be very important for high performance applications).

## Name
The name `ferrite` comes from: [Ferrite Magnet](http://en.wikipedia.org/wiki/Ferrite_\(magnet\))

## Install
### Requirements
 * [Kyoto Cabinet](http://fallabs.com/kyotocabinet/pkg/) - built and tested with version `1.2.76`
 * [libcurl](http://curl.haxx.se/libcurl/) - built and tested with version `7.21.4`

```bash
git clone git://github.com/brettlangdon/ferrite.git
cd ./ferrite
make release
make install
```

## Using
### Usage
```bash
$ ferrite --help
Usage: ferrite [-h|--help] [-p|--port <NUM>] [-w|--workers <NUM>] [-u|--url <STRING>] [-c|--cache <STRING>] [-e|--expire <NUM>]
    -p, --port    - which port number to bind too [default: 7000]
    -w, --workers - how many background workers to spawn [default: 10]
    -u, --url     - which url to proxy requests to [default: http://127.0.0.1:8000]
    -c, --cache   - kyoto cabinet cache to use [default: "*"]
    -e, --exp     - the expiration time in seconds from when a record is cached, 0 to disable [default:3600]
    -h, --help    - display this message
```

### Memcache Client
Just use your favorite memcache client
```python
import pymemcache.client
mc = pymemcache.client.Client([("127.0.0.1", 7000)])
users = mc.get("/all/users")
```

### Telnet
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

## License
```
The MIT License (MIT)

Copyright (c) 2013 Brett Langdon

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
