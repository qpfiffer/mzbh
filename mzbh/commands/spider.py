import click
import logging
import os
import time
import re
import binascii
import hashlib
import psycopg2
import sqlalchemy
from datetime import datetime
from flask import Flask
from flask.cli import with_appcontext
from mzbh.database import db
from mzbh.models import Webm, WebmAlias, Category
from os.path import join, getsize, islink, exists

PATH = "./webms/"

logging.basicConfig(filename='/tmp/mzbh.spider.txt', level=logging.DEBUG)
log = logging.getLogger(__name__)

@click.command('spider')
@with_appcontext
def spider():
    log.info(f"Starting spider on {PATH}.")
    bad_links = 0
    good_links = 0
    webm_aliases_total = 0
    webm_aliases_in_db_total = WebmAlias.query.count()

    known_webms = 0
    unknown_webms = 0
    webms_in_db_total = Webm.query.count()
    for root, dirs, files in os.walk(PATH):
        # 1. Check if is symlink, and is broken.
        for filename in files:
            if filename.startswith("thumb_"):
                continue

            file_path = join(root, filename)
            if islink(file_path):
                webm_aliases_total += 1
                if exists(file_path):
                    good_links += 1
                    # Check the DB for this alias.
                    exists_in_db = WebmAlias.query.filter_by(file_path=file_path).count() > 0
                    if not exists_in_db:
                        log.info(f"{file_path} is a good link, but does not exist in DB.")
                else:
                    bad_links += 1
                    log.warning(f"{file_path} is a bad link.")
            else:
                exists_in_db = Webm.query.filter_by(file_path=file_path).count() > 0
                if not exists_in_db:
                    unknown_webms += 1
                    tim = datetime.fromtimestamp(os.path.getctime(file_path))
                    split_name = filename.split("_")
                    unwebm = filename.split(".webm")[0]
                    category = Category.query.filter_by(name=root.split("/")[2]).first()
                    try:
                        original_filename = re.search(r"^[a-zA-Z_]*[0-9]*?_(.*).webm", filename).groups(0)[0]
                    except AttributeError:
                        log.error("Fucked up filename, skipping: {}".format(filename))
                        continue
                    log.info(f"Creating unknown Webm: {tim}: {file_path} as {original_filename}.")

                    md5_hash = hashlib.md5()
                    with open(file_path, "rb") as a_file:
                        content = a_file.read()
                        md5_hash.update(content)

                    digest = md5_hash.hexdigest()
                    # 4chan does this: binascii.hexlify(binascii.a2b_base64(thing))
                    file_hash = binascii.b2a_base64(binascii.unhexlify(digest)).strip().decode()
                    new_webm = Webm(
                        created_at=tim,
                        category_id=category.id,
                        old_id=-1,
                        file_hash=file_hash,
                        post_id = None,
                        file_path=file_path,
                        filename=filename,
                        size=0,
                    )
                    try:
                        db.session.add(new_webm)
                        db.session.commit()
                    except (psycopg2.errors.UniqueViolation, sqlalchemy.exc.IntegrityError,):
                        log.error("Filehash violation for file: {}".format(file_path))
                        db.session.rollback()
                else:
                    known_webms += 1
    log.info(f"Webm Aliases: Found {bad_links + good_links} on filesystem out of {webm_aliases_in_db_total} in the database.")
    log.info(f"Webm Aliases: {bad_links}/{webm_aliases_total} were bad.")
    log.info(f"Webm Aliases: {good_links}/{webm_aliases_total} were good.")

    log.info(f"Webm: Found {known_webms + unknown_webms} on the filesystem out of {webms_in_db_total} in the database.")
    log.info(f"Webm: {known_webms}/{webms_in_db_total} were known.")
    log.info(f"Webm: {unknown_webms}/{webms_in_db_total} were unknown.")
