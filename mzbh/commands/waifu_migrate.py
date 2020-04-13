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

waifu_conn = psycopg2.connect("dbname=waifu")
waifu_conn.autocommit = False

mzbh_conn = psycopg2.connect("dbname=mzbh")
mzbh_conn.autocommit = False

# 1. Alter table <foo> add old_id = waifu_table.id;

thread_fetch_sql = 'SELECT * FROM threads;'
def _do_threads(mzbh_cur, waifu_cur):
    mzbh_cur.execute("ALTER TABLE mzbh.thread ADD COLUMN old_id INTEGER;")
    host, _ = get_or_create(Host, name="4chan")
    waifu_cur.execute(thread_fetch_sql)
    row = waifu_cur.fetchone()
    while row:
        board = row["board"]
        subject = row["subject"]
        category, _ = get_or_create(Category, name=board, host_id=host.id)
        thread_obj, _ = get_or_create(Thread, category_id=category.id, thread_ident=t_num, subject=subject)
        # HAve to add old_id to the ORM thinger, too.
        row = waifu_cur.fetchone()

post_fetch_sql = 'SELECT * FROM posts;'
def _do_posts(mzbh_cur, waifu_cur):
    cur.execute(post_fetch_sql)
    row = cur.fetchone()
    while row:
        # Make thread
        row = cur.fetchone()

webm_fetch_sql = 'SELECT * FROM webms;'
def _do_webms(mzbh_cur, waifu_cur):
    cur.execute(webm_fetch_sql)
    row = cur.fetchone()
    while row:
        # Make thread
        row = cur.fetchone()

webm_alias_fetch_sql = 'SELECT * FROM webm_aliases;'
def _do_aliases(mzbh_cur, waifu_cur):
    cur.execute(webm_alias_fetch_sql)
    row = cur.fetchone()
    while row:
        # Make thread
        row = cur.fetchone()

@click.command('waifu_migrate')
@with_appcontext
def waifu_migrate():
    try:
        with waifu_conn.cursor() as waifu_cur, mzbh_conn.cursor() as mzbh_cur:
            mzbh_cur.execute("ALTER TABLE mzbh.thread ADD COLUMN old_id INTEGER;")
            # Repeat the add column for all tables ^^^
            _do_threads(mzbh_cur, waifu_cur)
            _do_posts(mzbh_cur, waifu_cur)
            _do_webms(mzbh_cur, waifu_cur)
            _do_aliases(mzbh_cur, waifu_cur)
            cur.execute("ALTER TABLE mzbh.thread DROP COLUMN old_id;")
            # Repeat the drop column for all tables ^^^
    except Exception as e:
        print("Exception: {e}")
        conn.rollback()
    conn.close()
