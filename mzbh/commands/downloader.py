import click
import ffmpeg
import os
import requests
from datetime import datetime, timezone
from flask import Flask
from flask.cli import with_appcontext
from mzbh.database import db
from mzbh.filters import _get_thumb_filename
from mzbh.models import Category, Host, Post, Thread, Webm, WebmAlias
from mzbh.utils import get_or_create

BOARDS = ("a", "b", "fit", "g", "gif", "e", "h", "o", "n", "r", "s", "sci", "soc", "v", "wsg",)
WEBMS_DIR = "./webms"


def _ensure_dir(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)


def _create_index_for_catalog(board):
    catalog = requests.get("https://a.4cdn.org/{}/catalog.json".format(board))
    catalog = catalog.json()

    matches = []
    for page in catalog:
        threads = page["threads"]
        for thread in threads:
            replies = thread.get("last_replies", [])
            thread_num = thread["no"]
            file_ext = thread.get("ext", None)
            post = thread.get("com", "")

            try:
                post_is_webm = file_ext and ".webm" in file_ext
                gif_in_post = "gif" in thread["com"]
                webm_in_post = "webm" in thread["com"]

                if post_is_webm or gif_in_post or webm_in_post:
                    matches.append(thread_num)
                    continue
            except KeyError:
                pass

            for thread_reply in replies:
                reply_ext = thread_reply.get("ext", None)
                gif_in_reply = "gif" in post.lower()
                webm_in_reply = "webm" in post.lower()
                if reply_ext and ".webm" in reply_ext:
                    matches.append(thread_num)
                    break
    return matches


def _download(url, filename):
    r = requests.get(url, stream=True)
    with open(filename, 'wb') as f:
        for chunk in r.iter_content(chunk_size=1024):
            if chunk:
                f.write(chunk)


def _thumbnail(filepath):
    outname, path = _get_thumb_filename(filepath)
    _ensure_dir(path)
    outname = path + outname
    (
        ffmpeg
        .input(filepath)
        .output(outname, **{'qscale:v': 3, 'frames:v': 1})
        .run()
    )

@click.command('downloader_4ch')
@with_appcontext
def downloader_4ch():
    host, _ = get_or_create(Host, name="4chan")
    _ensure_dir(WEBMS_DIR)
    for board in BOARDS:
        category, _ = get_or_create(Category, name=board, host_id=host.id)
        thread_nums_with_maybe_matches = _create_index_for_catalog(board)

        for t_num in thread_nums_with_maybe_matches:
            _ensure_dir(os.path.join(WEBMS_DIR, board))

            thread_data = requests.get("https://a.4cdn.org/{}/thread/{}.json".format(board, t_num))
            thread_data = thread_data.json()

            thread_obj, _ = get_or_create(Thread, category_id=category.id, thread_ident=t_num, subject=thread_data.get("sub", None))
            for post in thread_data["posts"]:
                valuable_data = {
                    "file_ext": post.get("ext", None),
                    "filename": post.get("filename", ""),
                    "no": post["no"],
                    "siz": post.get("fsize", 0),
                    "_tim": post.get("tim", None),
                    "body_content": post.get("com", ""),
                    "md5sum": post.get("md5", None)
                }

                post_time = post["time"]
                post_time = datetime.fromtimestamp(post["time"]).astimezone(timezone.utc)
                post_obj, _ = get_or_create(Post, thread_id=thread_obj.id, posted_at=post_time,
                    source_post_id=post["no"],
                    body_content=valuable_data["body_content"],
                )

                if valuable_data["file_ext"] is None:
                    continue

                md5 = valuable_data["md5sum"]
                filename = valuable_data["filename"]
                ext = valuable_data["file_ext"]
                if md5 and ext == ".webm":
                    # Do download this one.
                    if Webm.query.filter_by(file_hash=md5).count() > 0:
                        if WebmAlias.query.filter_by(
                                post_id=post_obj.id,
                                file_hash=md5,
                                category_id=category.id,
                                filename=filename).count() > 0:
                            print("Skipping {}".format(filename))
                            continue
                        else:
                            print("Found new alias: {}".format(filename))
                            webm = Webm.query.filter_by(file_hash=md5).first()
                            alias = WebmAlias(
                                filename=filename,
                                file_hash=md5,
                                post_id=post_obj.id,
                                category_id=category.id,
                                webm_id=webm.id)
                            db.session.add(alias)
                            db.session.commit()
                    else:
                        # DOWNLOAD WEBM!
                        filename_len = len(filename)
                        all_else_len = len(board) + len(valuable_data["siz"]) + len(valuable_data["file_ext"])
                        new_filename_len = 250 - all_else_len

                        non_collideable_name = "{}_{}_{}{}".format(board,
                            valuable_data["siz"],
                            filename[:new_filename_len],
                            valuable_data["file_ext"]
                        )
                        filepath = os.path.join(WEBMS_DIR, board, non_collideable_name)
                        url = "https://i.4cdn.org/{}/{}{}".format(board, post["tim"], post["ext"])
                        print("Downloading {}".format(valuable_data["filename"]))
                        _download(url, filepath)
                        _thumbnail(filepath)
                        webm = Webm(file_hash=md5,
                            filename=valuable_data["filename"],
                            file_path=filepath,
                            post_id=post_obj.id,
                            category_id=category.id,
                            size=valuable_data["siz"],
                        )
                        db.session.add(webm)
                        db.session.commit()
