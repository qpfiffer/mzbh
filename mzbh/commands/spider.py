import click
import logging
import os
from flask import Flask
from flask.cli import with_appcontext
from mzbh.models import Webm, WebmAlias
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
                    log.info(f"{file_path} is a good Webm, but does not exist in DB.")
                else:
                    known_webms += 1
    log.info(f"Webm Aliases: Found {bad_links + good_links} on filesystem out of {webm_aliases_in_db_total} in the database.")
    log.info(f"Webm Aliases: {bad_links}/{webm_aliases_total} were bad.")
    log.info(f"Webm Aliases: {good_links}/{webm_aliases_total} were good.")

    log.info(f"Webm: Found {known_webms + unknown_webms} on the filesystem out of {webms_in_db_total} in the database.")
    log.info(f"Webm: {known_webms}/{webms_in_db_total} were known.")
    log.info(f"Webm: {unknown_webms}/{webms_in_db_total} were unknown.")
