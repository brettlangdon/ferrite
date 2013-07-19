fast-cache
==========

A very fast kyoto cabinet powered memcache interface http proxy caching server.


Very rough done benchmark so far, just running against a local wsgi app with 5 gunicorn workers returning `{}` only.
```python
import random
import time

import memcache

mc = memcache.Client(["127.0.0.1:7000"])
# hack to make sure we force the connection before the first call
for server in mc.servers:
    server.connect()

elapsed = 0
total = 0
numbers = range(5000)
for _ in range(100000):
    start = time.time()
    result = mc.get("test:%s" % random.choice(numbers))
    end = time.time()
    if result is None:
        break
    elapsed += end - start
    total += 1

print "Total: %s" % total
print "Total Elapsed: %s" % elapsed
print "Average: %s" % (elapsed / total)
print "Req/sec: %s" % (total / elapsed)

```

Fresh cache:
```bash
╭─brett@Voltaire  ~/Desktop/fast-cache  ‹master›
╰─$ python bench.py                                                                                          
Total: 100000
Total Elapsed: 21.179005146
Average: 0.00021179005146
Req/sec: 4721.65709912
```

Second Run:
```bash
╭─brett@Voltaire  ~/Desktop/fast-cache  ‹master›
╰─$ python bench.py
Total: 100000
Total Elapsed: 14.9816277027
Average: 0.000149816277027
Req/sec: 6674.84214562
```

`STATS`
```bash
╭─brett@Voltaire  ~/Desktop/fast-cache  ‹master›
╰─$ telnet 127.0.0.1 7000
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
STATS
STAT cache_miss 5000
STAT hits 195000
STAT hit_ratio 0.98
STAT records 5000
END
```
