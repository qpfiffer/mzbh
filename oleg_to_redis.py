#!/usr/bin/env python2
from olegdb import OlegDB
import redis

namespaces = ["webm", "alias", "W2A", "PST", "THRD"]

def main():
    redis_c = redis.StrictRedis(host='localhost', port=6379, db=0)
    olegdb_c = OlegDB(db_name="waifu")
    for namespace in namespaces:
        values = olegdb_c.get_many(olegdb_c.get_by_prefix(namespace))
        for key in values:
            value = values[key]
            redis_c.set(key, value)
    return 0

if __name__ == '__main__':
    main()
