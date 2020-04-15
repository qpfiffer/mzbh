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
    host, _ = get_or_create(Host, name="4chan")
    waifu_cur.execute(thread_fetch_sql)
    row = waifu_cur.fetchone()
    while row:
        old_id = row[0]
        board = row[2]
        subject = row[3]
        created_at = row[4]
        print("Migrating Thread {} ({}), {} - {}".format(old_id, created_at, board, subject))
        category, _ = get_or_create(Category, name=board, host_id=host.id)
        thread_obj, _ = get_or_create(Thread, category_id=category.id, old_id=old_id, created_at=created_at, subject=subject)
        row = waifu_cur.fetchone()

post_fetch_sql = 'SELECT * FROM posts;'
def _do_posts(mzbh_cur, waifu_cur):
    host, _ = get_or_create(Host, name="4chan")
    waifu_cur.execute(post_fetch_sql)
    row = waifu_cur.fetchone()
    while row:
        board = row[5]
        old_id = row[0]
        created_at = row[8]
        source_post_id = row[2]
        replied_to_keys = row[7]
        body_content = row[6]
        thread_id = row[4]

        old_thread = Thread.query.filter_by(old_id=thread_id).first()
        old_thread_id = None
        if old_thread:
            old_thread_id = old_thread.id

        print("Migrating Post {} ({}), {} - {}".format(old_id, created_at, source_post_id, body_content[:10] if body_content else "N/A"))
        category, _ = get_or_create(Category,
                name=board,
                host_id=host.id)
        post_obj, _ = get_or_create(
                Post,
                thread_id=old_thread_id,
                old_id=old_id,
                created_at=created_at,
                posted_at=created_at,
                source_post_id=source_post_id,
                replied_to_keys=replied_to_keys,
                body_content=body_content,
        )
        row = waifu_cur.fetchone()

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
            #mzbh_cur.execute("ALTER TABLE mzbh.thread ADD COLUMN old_id INTEGER;")
            #mzbh_cur.execute("ALTER TABLE mzbh.post ADD COLUMN old_id INTEGER;")
            #mzbh_conn.commit()
            # Repeat the add column for all tables ^^^
            _do_threads(mzbh_cur, waifu_cur)
            _do_posts(mzbh_cur, waifu_cur)
            mzbh_conn.rollback()
            _do_webms(mzbh_cur, waifu_cur)
            _do_aliases(mzbh_cur, waifu_cur)
            mzbh_cur.execute("ALTER TABLE mzbh.thread DROP COLUMN old_id;")
            mzbh_cur.execute("ALTER TABLE mzbh.post DROP COLUMN old_id;")
            mzbh_conn.rollback()
            #mzbh_conn.commit()
    except Exception as e:
        print(f"Exception: {e}")
        mzbh_conn.rollback()
    waifu_conn.close()
    mzbh_conn.close()
