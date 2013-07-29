ferrite
==========

A very fast kyoto cabinet powered memcached interface API proxy caching server.

This is not your average caching proxy server. For starters it uses the [memcached protocol](https://github.com/memcached/memcached/blob/master/doc/protocol.txt)
rather than normal HTTP (this is because the memcached protocol is more light weight than HTTP and we are shooting for performance).
As well, a notable difference is that when there is a cache miss, the call to the proxy is started as a background process
and an empty response is sent back to the client. This is because we would rather have no value and a consistent lookup time
from the cache than have the client wait for a response from the proxy.

## Name
The name `ferrite` comes from: [Ferrite Magnet](http://en.wikipedia.org/wiki/Ferrite_\(magnet\))

## Requirements
 * [Kyoto Cabinet](http://fallabs.com/kyotocabinet/pkg/) - built and tested with version `1.2.76`
 * [Go](http://golang.org/)

## Installation
```bash
go get github.com/brettlangdon/ferrite
```

## Usage
### Running
```bash
$ ferrite --help
Usage of ferrite:
  -bind="0.0.0.0:7000": the [<address>]:<port> to bind to
  -cache="*": the kyoto cabinet cache path
  -ttl=3600: the TTL in seconds for a newly cached object
```

For the `-cache` parameter please refer to the Kyoto Tycoon [ktserver](http://fallabs.com/kyototycoon/command.html#ktserver) database naming convention for possible options.

```
The naming convention of database name is the same as polymorphic database of Kyoto Cabinet.
If it is "-", the database will be a prototype hash database. If it is "+", the database will
be a prototype tree database. If it is ":", the database will be a stash database. If it is "*",
the database will be a cache hash database. If it is "%", the database will be a cache tree database.
If its suffix is ".kch", the database will be a file hash database. If its suffix is ".kct",
the database will be a file tree database. If its suffix is ".kcd", the database will be a directory
hash database. If its suffix is ".kcf", the database will be a directory tree database. Tuning parameters
can trail the name, separated by "#". Each parameter is composed of the name and the value, separated by
"=". If the "type" parameter is specified, the database type is determined by the value in "-", "+", ":",
"*", "%", "kch", "kct", "kcd", and "kcf". All database types support the logging parameters of "log",
"logkinds", and "logpx". The prototype hash database and the prototype tree database do not support any
other tuning parameter. The stash database supports "bnum". The cache hash database supports "opts", "bnum",
"zcomp", "capcnt", "capsiz", and "zkey". The cache tree database supports all parameters of the cache hash
database except for capacity limitation, and supports "psiz", "rcomp", "pccap" in addition. The file hash
database supports "apow", "fpow", "opts", "bnum", "msiz", "dfunit", "zcomp", and "zkey". The file tree
database supports all parameters of the file hash database and "psiz", "rcomp", "pccap" in addition.
The directory hash database supports "opts", "zcomp", and "zkey". The directory tree database supports all
parameters of the directory hash database and "psiz", "rcomp", "pccap" in addition.

Furthermore, several parameters are added by Kyoto Tycoon. "ktopts" sets options and the value can contain
"p" for the persistent option. "ktcapcnt" sets the capacity by record number. "ktcapsiz" sets the capacity
by database size.
```

### Commands
```bash
$ telnet 127.0.0.1 7000
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
help
COMMAND GET <KEY> - retrieve <KEY> from the cache
COMMAND FLUSH_ALL - remove all records from the cache
COMMAND DELETE <KEY> - remove <KEY> from the cache
COMMAND VERSION - display the application version
COMMAND STATS - display application statistics
COMMAND HELP - display this information
END
```

### Example
```bash
$ telnet 127.0.0.1 7000
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
get http://127.0.0.1:8000/test
END
get http://127.0.0.1:8000/test
VALUE http://127.0.0.1:8000/test 0 2
{}
END
```
### Escaped URLs
`ferrite` will unescape all urls which begin with `http%3a%2f%2f` or `https%3a%2f%2f`.
```bash
$ telnet 127.0.0.1 7000
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
get http%3A%2F%2F127.0.0.1%3A8000%2Ftest
END
get http%3A%2F%2F127.0.0.1%3A8000%2Ftest
VALUE http://127.0.0.1:8000/test 0 2
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
