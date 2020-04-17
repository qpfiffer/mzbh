import click
import ffmpeg
import logging
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

logging.basicConfig(level=logging.DEBUG)
log = logging.getLogger(__name__)

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
        log.info("Migrating Thread {} ({}), {} - {}".format(old_id, created_at, board, subject))
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
        new_thread_id = None
        if old_thread:
            new_thread_id = old_thread.id

        log.info("Migrating Post {} ({}), {} - {}".format(old_id, created_at, source_post_id, body_content[:10] if body_content else "N/A"))
        category, _ = get_or_create(Category,
                name=board,
                host_id=host.id)
        post_obj, _ = get_or_create(
                Post,
                thread_id=new_thread_id,
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
    host, _ = get_or_create(Host, name="4chan")
    waifu_cur.execute(webm_fetch_sql)
    row = waifu_cur.fetchone()
    while row:
        old_id = row[0]
        oleg_key = row[1]
        file_hash = row[2]
        filename = row[3]
        board = row[4]
        file_path = row[5]
        post_id = row[6]
        created_at = row[7]
        size = row[8]

        old_post = Post.query.filter_by(old_id=post_id).first()
        new_post_id = None
        if old_post:
            new_post_id = old_post.id

        log.info("Migrating Webm {} ({}), {}".format(old_id, created_at, filename))
        category, _ = get_or_create(Category,
                name=board,
                host_id=host.id)
        webm_obj, _ = get_or_create(
                Webm,
                old_id=old_id,
                created_at=created_at,
                oleg_key = oleg_key,
                file_hash = file_hash,
                filename = filename,
                category_id = category.id,
                file_path = file_path,
                post_id = new_post_id,
                size = size,
        )
        row = waifu_cur.fetchone()

webm_alias_fetch_sql = 'SELECT * FROM webm_aliases;'
def _do_aliases(mzbh_cur, waifu_cur):
    host, _ = get_or_create(Host, name="4chan")
    waifu_cur.execute(webm_alias_fetch_sql)
    row = waifu_cur.fetchone()
    while row:
        old_id = row[0]
        oleg_key = row[1]
        file_hash = row[2]
        filename = row[3]
        board = row[4]
        file_path = row[5]
        post_id = row[6]
        webm_id = row[7]
        created_at = row[8]

        old_post = Post.query.filter_by(old_id=post_id).first()
        new_post_id = None
        if old_post:
            new_post_id = old_post.id

        old_webm = Webm.query.filter_by(old_id=webm_id).first()
        new_webm_id = None
        if old_webm:
            new_webm_id = old_webm.id

        log.info("Migrating Webm Alias {} ({}), {}".format(old_id, created_at, filename))
        category, _ = get_or_create(Category,
                name=board,
                host_id=host.id)
        webm_alias_obj, _ = get_or_create(
                WebmAlias,
                old_id=old_id,
                created_at=created_at,
                oleg_key = oleg_key,
                file_hash = file_hash,
                filename = filename,
                category_id = category.id,
                file_path = file_path,
                post_id = new_post_id,
                webm_id = new_webm_id,
        )
        row = waifu_cur.fetchone()

@click.command('waifu_migrate')
@with_appcontext
def waifu_migrate():
    try:
        with waifu_conn.cursor() as waifu_cur, mzbh_conn.cursor() as mzbh_cur:
            mzbh_cur.execute("ALTER TABLE mzbh.thread ADD COLUMN old_id INTEGER;")
            mzbh_cur.execute("ALTER TABLE mzbh.post ADD COLUMN old_id INTEGER;")
            mzbh_cur.execute("ALTER TABLE mzbh.webm ADD COLUMN old_id INTEGER;")
            mzbh_cur.execute("ALTER TABLE mzbh.webm_alias ADD COLUMN old_id INTEGER;")
            mzbh_conn.commit()
            _do_threads(mzbh_cur, waifu_cur)
            _do_posts(mzbh_cur, waifu_cur)
            _do_webms(mzbh_cur, waifu_cur)
            _do_aliases(mzbh_cur, waifu_cur)
            #mzbh_cur.execute("ALTER TABLE mzbh.thread DROP COLUMN old_id;")
            #mzbh_cur.execute("ALTER TABLE mzbh.post DROP COLUMN old_id;")
            #mzbh_cur.execute("ALTER TABLE mzbh.webm DROP COLUMN old_id;")
            #mzbh_cur.execute("ALTER TABLE mzbh.webm_alias DROP COLUMN old_id;")
            #mzbh_conn.rollback()
            mzbh_conn.commit()
    except Exception as e:
        print(f"Exception: {e}")
        mzbh_conn.rollback()
    waifu_conn.close()
    mzbh_conn.close()
