import click
import ffmpeg
import os
import psycopg2
import requests
from datetime import datetime, timezone
from flask import Flask
from flask.cli import with_appcontext
from mzbh.database import db
from mzbh.filters import _get_thumb_filename
from mzbh.models import Category, Host, Post, Thread, Webm, WebmAlias
from mzbh.utils import get_or_create

conn = psycopg2.connect("dbname=waifu")
conn.autocommit = False

thread_fetch_sql = 'SELECT * FROM threads;'
def _do_threads(cur):
    host, _ = get_or_create(Host, name="4chan")
    cur.execute(thread_fetch_sql)
    row = cur.fetchone()
    while row:
        board = row["board"]
        subject = row["subject"]
        category, _ = get_or_create(Category, name=board, host_id=host.id)
        thread_obj, _ = get_or_create(Thread, category_id=category.id, thread_ident=t_num, subject=subject)
        # Aw shit we have to be able to tie the old IDs back to the webms/aliases.
        # Probably with some temporary "old_id" column so we can correlate everything.
        row = cur.fetchone()

post_fetch_sql = 'SELECT * FROM posts;'
def _do_posts(cur):
    cur.execute(post_fetch_sql)
    row = cur.fetchone()
    while row:
        # Make thread
        row = cur.fetchone()

webm_fetch_sql = 'SELECT * FROM webms;'
def _do_webms(cur):
    cur.execute(webm_fetch_sql)
    row = cur.fetchone()
    while row:
        # Make thread
        row = cur.fetchone()

webm_alias_fetch_sql = 'SELECT * FROM webm_aliases;'
def _do_aliases(cur):
    cur.execute(webm_alias_fetch_sql)
    row = cur.fetchone()
    while row:
        # Make thread
        row = cur.fetchone()

@click.command('waifu_migrate')
@with_appcontext
def waifu_migrate():
    try:
        with conn.cursor() as cur:
            _do_threads(cur)
            _do_posts(cur)
            _do_webms(cur)
            _do_aliases(cur)
    except Exception as e:
        print("Exception: {e}")
        conn.rollback()
    conn.close()
