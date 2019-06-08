#!/usr/bin/env python3
from olegdb.oleg import OlegDB
import json, psycopg2, time, multiprocessing

olegdb_c = OlegDB(db_name="waifu")

def insert_post(conn, oconn, okey, od, should_commit=True):
    cur = conn.cursor()
    try:
        pl = json.loads(od.decode(), strict=False)
    except Exception as e:
        print("Could not load Post {}.".format(okey))
        print("E: {}".format(e))
        print("Raw Data: {}".format(od))
        return

    body_content = pl.get('body_content', None)
    post_no = pl.get('post_no', None)
    if post_no:
        post_no = int(post_no)

    created_at = pl.get('created_at', None)
    if not created_at:
        created_at = time.time()

    cur.execute("select count(*) from posts where oleg_key = '{}'".format(okey));
    exists = cur.fetchone()[0]

    if exists != 0:
        return

    t = time.time()
    threaddata = (pl['board'], 'THRDoleg2pg{}{}'.format(pl['board'], t), t)
    cur.execute("""INSERT INTO threads
        (board, oleg_key, created_at)
        VALUES (%s, %s, to_timestamp('%s')) RETURNING id;""", threaddata)
    thread_id = cur.fetchone()[0]

    pdata = (created_at, okey, post_no, int(pl['post_id']),
            pl['board'], body_content, json.dumps(pl['replied_to_keys']),
            thread_id)
    cur.execute("""INSERT INTO posts
        (created_at, oleg_key, fourchan_post_no, fourchan_post_id, board, body_content, replied_to_keys, thread_id)
        VALUES (to_timestamp('%s'), %s, %s, %s, %s, %s, %s, %s) RETURNING id;""", pdata)

namespaces = (
    ("PST", insert_post),
)

def map_func(quesy):
    dec = quesy.decode()
    if ":PST" not in dec:
        return

    key = dec.split(":")[4]
    conn = psycopg2.connect("dbname=waifu")
    od = olegdb_c.get(key)
    insert_post(conn, olegdb_c, key, od)

def main():
    pool = multiprocessing.Pool()

    with open("./data/waifu.aol", 'rb') as data_file:
        for namespace, namespace_handler in namespaces:
            pool.map(map_func, data_file)

    pool.close()
    pool.join()
    return 0

if __name__ == '__main__':
    main()
