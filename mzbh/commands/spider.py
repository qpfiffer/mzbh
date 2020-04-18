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
    for root, dirs, files in os.walk(PATH):
        # 1. Check if is symlink, and is broken.
        for filename in files:
            if filename.startswith("thumb_"):
                continue

            file_path = join(root, filename)
            if islink(file_path):
                if exists(file_path):
                    # Check the DB for this alias.
                    exists_in_db = WebmAlias.query.filter_by(file_path=file_path).count() > 0
                    if not exists_in_db:
                        log.info(f"{file_path} is a good link, but does not exist in DB.")
                else:
                    log.warning(f"{file_path} is a bad link.")
            else:
                exists_in_db = Webm.query.filter_by(file_path=file_path).count() > 0
                if not exists_in_db:
                    log.info(f"{file_path} is a good Webm, but does not exist in DB.")
