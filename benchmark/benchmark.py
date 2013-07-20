from multiprocessing import Process
import random
import time

import memcache


def get_next(num):
    numbers = range(num)
    while True:
        yield random.choice(numbers)


def test(calls=1000, keys=100):
    print "Starting Test"
    mc = memcache.Client(["127.0.0.1:7000"])
    # hack to make sure we force the connection before the first call
    for server in mc.servers:
        server.connect()

    elapsed = 0
    total = 0
    numbers = get_next(keys)
    hits = 0
    miss = 0
    for _ in range(calls):
        num = numbers.next()
        key = "test:%s" % num
        start = time.time()
        result = mc.get(key)
        end = time.time()
        if not result:
            miss += 1
        else:
            hits += 1
        elapsed += end - start
        total += 1

    print "Total: %s" % total
    print "Total Elapsed: %s" % elapsed
    print "Average: %s" % (elapsed / total)
    print "Req/sec: %s" % (total / elapsed)
    print "Hits: %s" % hits
    print "Miss: %s" % miss
    print "Hit Ratio: %s" % (float(hits) / float(hits + miss))


procs = []
for _ in range(10):
    p = Process(target=test, args=(100000, 5000))
    p.start()
    procs.append(p)

for p in procs:
    p.join()
