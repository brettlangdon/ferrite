from multiprocessing import Process, Queue
import random
import time

import pymemcache.client


def get_next(num):
    numbers = range(num)
    while True:
        yield random.choice(numbers)


def test(results, calls=1000, keys=100):
    mc = pymemcache.client.Client(("127.0.0.1", 7000))

    elapsed = 0
    total = 0
    numbers = get_next(keys)
    hits = 0
    miss = 0
    for _ in range(calls):
        num = numbers.next()
        key = "%s" % num
        start = time.time()
        result = mc.get(key)
        end = time.time()
        if not result:
            miss += 1
        else:
            hits += 1
        elapsed += end - start
        total += 1

    results.put((total, hits, miss))


requests_per_process = 100000
possible_urls = 5000
workers = 10
procs = []
results = Queue()
start = time.time()

print "Starting %s Workers" % workers
for _ in range(workers):
    p = Process(target=test, args=(results, requests_per_process, possible_urls))
    p.start()
    procs.append(p)

for p in procs:
    p.join()
end = time.time()

elapsed = end - start
total = 0
hits = 0
miss = 0
while not results.empty():
    one_total, one_hits, one_miss = results.get()
    total += one_total
    hits += one_hits
    miss += one_miss

print "=" * 30
print "Num Workers:\t\t%s" % workers
print "Expected Requests:\t%s" % (requests_per_process * workers)
print "Actual Request:\t\t%s" % total
print "Elapsed Time (sec):\t%2.4f" % elapsed
print "Total Reqs/Sec:\t\t%.4f" % (total / elapsed)
print "Req/Sec Per Worker:\t%.4f" % ((total / elapsed) / workers)
print "Hits:\t\t\t%s" % hits
print "Misses:\t\t\t%s" % miss
print "Hit Ratio:\t\t%2.4f" % (float(hits) / float(total))
print "=" * 30
