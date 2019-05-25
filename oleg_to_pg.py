#!/usr/bin/env python
from olegdb.oleg import OlegDB
import json, psycopg2, time

# Left to do:
# 1. Check that files exist when we insert webms and aliases

def insert_webm(conn, oconn, okey, od, should_commit):
    cur = conn.cursor()
    webm_loaded = json.loads(od.decode())
    webm_data = (webm_loaded['created_at'], okey, webm_loaded['filename'],
            webm_loaded['file_hash'], webm_loaded['board'],
            webm_loaded['file_path'], webm_loaded['size'])
    cur.execute("""INSERT INTO webms
        (created_at, oleg_key, filename, file_hash, board,
        file_path, size)
        VALUES (to_timestamp('%s'), %s, %s, %s, %s, %s, %s) RETURNING id;""", webm_data)
    post_id = webm_loaded.get("post", None)
    if post_id and len(post_id):
        webm_id = cur.fetchone()[0]
        cur.execute("UPDATE webms SET post_id = (SELECT id FROM posts WHERE oleg_key = %s) WHERE webms.id = %s", [post_id, webm_id])
    if should_commit:
        conn.commit()

def insert_alias(conn, oconn, okey, od, should_commit):
    cur = conn.cursor()
    str_od = od.decode()
    webm_loaded = json.loads(str_od)
    webm_data = (webm_loaded['created_at'], okey, webm_loaded['filename'],
            webm_loaded['file_hash'], webm_loaded['board'],
            webm_loaded['file_path'], webm_loaded['file_hash'])
    cur.execute("""INSERT INTO webm_aliases
        (created_at, oleg_key, filename, file_hash, board, file_path, webm_id)
        VALUES (to_timestamp('%s'), %s, %s, %s, %s, %s, (SELECT id FROM webms WHERE file_hash = %s)) RETURNING id;""", webm_data)
    post_id = webm_loaded.get("post", None)
    if post_id and len(post_id):
        webm_alias_id = cur.fetchone()[0]
        cur.execute("UPDATE webm_aliases SET post_id = (SELECT id FROM posts WHERE oleg_key = %s) WHERE webm_aliases.id = %s", [post_id, webm_alias_id])
    if should_commit:
        conn.commit()

def insert_thread(conn, oconn, okey, od, should_commit):
    cur = conn.cursor()
    try:
        str_od = od.decode()
        thread_loaded = json.loads(str_od, strict=False)
    except ValueError as e:
        print("Could not load Thread {}.".format(okey))
        print("Raw Data: {}".format(od))
        raise e
    threaddata = (thread_loaded['board'], okey, time.time())
    cur.execute("""INSERT INTO threads
        (board, oleg_key, created_at)
        VALUES (%s, %s, to_timestamp('%s')) RETURNING id;""", threaddata)
    thread_id = cur.fetchone()[0]
    bulk_posts = oconn.get_many(thread_loaded['post_keys'])
    for post_key, post_value in bulk_posts.items():
        pl = json.loads(post_value.decode(), strict=False)
        body_content = pl.get('body_content', None)
        post_no = pl.get('post_no', None)
        if post_no:
            post_no = int(post_no)

        pdata = (time.time(), post_key, post_no, int(pl['post_id']),
                pl['board'], body_content, json.dumps(pl['replied_to_keys']),
                thread_id)
        cur.execute("""INSERT INTO posts
            (created_at, oleg_key, fourchan_post_no, fourchan_post_id, board, body_content, replied_to_keys, thread_id)
            VALUES (to_timestamp('%s'), %s, %s, %s, %s, %s, %s, %s) RETURNING id;""", pdata)

    if should_commit:
        conn.commit()

def create_tables(conn):
    cur = conn.cursor()
    cur.execute("""CREATE TABLE threads (
        id SERIAL PRIMARY KEY,
        oleg_key TEXT UNIQUE,
        board TEXT,
        subject TEXT,
        created_at TIMESTAMPTZ DEFAULT now());""")
    cur.execute("CREATE INDEX threads_oleg_key ON threads (oleg_key)")

    cur.execute("""CREATE TABLE posts (
        id SERIAL PRIMARY KEY,
        oleg_key TEXT UNIQUE,
        fourchan_post_id BIGINT, -- The date of the post (unsignedix timestamp)
        fourchan_post_no BIGINT, -- The actual post ID on 4chan
        thread_id INTEGER REFERENCES threads,
        board TEXT,
        body_content TEXT,
        replied_to_keys JSONB, -- JSON list of posts replied to ITT
        created_at TIMESTAMPTZ DEFAULT now());""")
    cur.execute("CREATE INDEX posts_oleg_key ON posts (oleg_key)")

    cur.execute("""CREATE TABLE webms (
        id SERIAL PRIMARY KEY,
        oleg_key TEXT UNIQUE,
        file_hash TEXT UNIQUE NOT NULL,
        filename TEXT,
        board TEXT,
        file_path TEXT,
        post_id INTEGER REFERENCES posts,
        created_at TIMESTAMPTZ DEFAULT now(),
        size INTEGER);""")
    cur.execute("CREATE INDEX webms_oleg_key ON webms (oleg_key)")
    cur.execute("CREATE INDEX webms_file_hash_idx ON webms (file_hash)")
    cur.execute("CREATE INDEX webms_filename_idx ON webms (filename)")

    cur.execute("""CREATE TABLE webm_aliases (
        id SERIAL PRIMARY KEY,
        oleg_key TEXT UNIQUE,
        file_hash TEXT NOT NULL,
        filename TEXT,
        board TEXT,
        file_path TEXT,
        post_id INTEGER REFERENCES posts,
        webm_id INTEGER REFERENCES webms,
        created_at TIMESTAMPTZ DEFAULT now());""")
    cur.execute("CREATE INDEX webm_aliases_oleg_key ON webms (oleg_key)")
    cur.execute("CREATE INDEX webm_aliases_file_hash_idx ON webms (file_hash)")
    cur.execute("CREATE INDEX webm_aliases_filename_idx ON webms (filename)")
    conn.commit()

namespaces = (
    ("THRD", insert_thread),
    ("webm", insert_webm),
    ("alias", insert_alias),
)

def main():
    should_commit = True
    if not should_commit:
        print("Dry run, not commiting...")

    conn = psycopg2.connect("dbname=waifu user=quinlan")
    create_tables(conn)
    olegdb_c = OlegDB(db_name="waifu")
    for namespace, namespace_handler in namespaces:
        values = olegdb_c.get_many(olegdb_c.get_by_prefix(namespace))
        for key, value in values.items():
            namespace_handler(conn, olegdb_c, key, value, should_commit)
    return 0

if __name__ == '__main__':
    main()
