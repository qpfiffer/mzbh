#!/usr/bin/env python2
from olegdb import OlegDB
import psycopg2

def insert_webm(conn, oleg_data):
    pass

def insert_alias(conn, oleg_data):
    pass

def insert_thread(conn, oleg_data):
    pass

def create_tables(conn):
    cur = conn.cursor()
    cur.execute("""CREATE TABLE threads (
        id SERIAL PRIMARY KEY,
        board TEXT,
        subject TEXT,
        created_at TIMESTAMPTZ);""")
    cur.execute("""CREATE TABLE posts (
        id SERIAL PRIMARY KEY,
        fourchan_post_id INTEGER, -- The date of the post (unsignedix timestamp)
        fourchan_post_no INTEGER, -- The actual post ID on 4chan
        thread_id INTEGER REFERENCES threads,
        -- webm_id INTEGER REFERENCES webms,
        board TEXT,
        body_content TEXT,
        replied_to_keys JSONB, -- JSON list of posts replied to ITT
        created_at TIMESTAMPTZ);""")
    cur.execute("""CREATE TABLE webms (
        id SERIAL PRIMARY KEY,
        file_hash TEXT,
        filename TEXT,
        board TEXT,
        file_path TEXT,
        post_id INTEGER REFERENCES posts,
        created_at TIMESTAMPTZ,
        size INTEGER);""")
    cur.execute("ALTER TABLE posts ADD COLUMN webm_id INTEGER REFERENCES webms;")
    cur.execute("""CREATE TABLE webm_aliases (
        id SERIAL PRIMARY KEY,
        file_hash TEXT,
        filename TEXT,
        board TEXT,
        file_path TEXT,
        post_id INTEGER REFERENCES posts,
        alias_of_id INTEGER REFERENCES webms,
        created_at TIMESTAMPTZ,
        size INTEGER);""")
    conn.commit()

namespaces = {
    "webm": insert_webm,
    "alias": insert_alias,
    "THRD": insert_thread
    }

def main():
    conn = psycopg2.connect("dbname=waifu user=quinlan")
    #create_tables(conn)
    olegdb_c = OlegDB(db_name="waifu")
    for namespace in namespaces:
        values = olegdb_c.get_many(olegdb_c.get_by_prefix(namespace))
        for key in values:
            value = values[key]
            redis_c.set(key, value)
    return 0

if __name__ == '__main__':
    main()
