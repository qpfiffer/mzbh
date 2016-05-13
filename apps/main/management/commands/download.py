from django.conf import settings
from django.core.management.base import BaseCommand, CommandError
from apps.main.models import Board, Thread, Post, Webm, WebmAlias

import os, requests

WEBMS_DIR = "./webms"

class Command(BaseCommand):
    help = 'Downloads porn from the internet.'

    def _ensure_dir(self, directory):
        if not os.path.exists(directory):
            os.makedirs(directory)

    def _create_index_for_catalog(self, board):
        catalog = requests.get("https://a.4cdn.org/{}/catalog.json".format(board.charname))
        catalog = catalog.json()

        matches = []
        for page in catalog:
            threads = page["threads"]
            for thread in threads:
                replies = thread.get("last_replies", [])
                thread_num = thread["no"]
                file_ext = thread["ext"]
                post = thread.get("com", "")

                found_webm_in_reply = False
                for thread_reply in replies:
                    reply_ext = thread_reply.get("ext", None)
                    if reply_ext and (
                            ".webm" in reply_ext or
                            "webm" in post.lower() or
                            "gif" in post.lower()):
                        found_webm_in_reply = True
                        matches.append(thread_num)
                        break
        return matches

    def _download(self, url, filename):
        r = requests.get(url, stream=True)
        with open(filename, 'wb') as f:
            for chunk in r.iter_content(chunk_size=1024):
                if chunk:
                    f.write(chunk)

    def handle(self, *args, **options):
        self._ensure_dir(WEBMS_DIR)
        for board in Board.objects.all():
            thread_nums_with_maybe_matches = self._create_index_for_catalog(board)

            for t_num in thread_nums_with_maybe_matches:
                self._ensure_dir(os.path.join(WEBMS_DIR, board.charname))

                thread_data = requests.get("https://a.4cdn.org/{}/thread/{}.json".format(board.charname, t_num))
                thread_data = thread_data.json()

                thread_obj, _ = Thread.objects.get_or_create(board=board, thread_ident=t_num)
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

                    post_obj, _ = Post.objects.get_or_create(post_id=post["time"],
                        post_no=post["no"],
                        body_content=valuable_data["body_content"],
                        thread=thread_obj)

                    if valuable_data["file_ext"] is None:
                        continue

                    md5 = valuable_data["md5sum"]
                    filename = valuable_data["filename"]
                    ext = valuable_data["file_ext"]
                    if md5 and ext == ".webm":
                        # Do download this one.
                        if Webm.objects.filter(filehash=md5).exists():
                            if WebmAlias.objects.filter(post=post_obj, filehash=md5, filename=filename).exists():
                                continue
                            else:
                                WebmAlias.objects.create(filehash=md5,
                                    filename=filename,
                                    post=post_obj,
                                    webm=Webm.objects.get(filehash=md5))
                        else:
                            # DOWNLOAD WEBM!
                            non_collideable_name = "{}_{}_{}{}".format(board.charname,
                                valuable_data["siz"],
                                filename,
                                valuable_data["file_ext"]
                            )
                            filepath = os.path.join(WEBMS_DIR, board.charname, non_collideable_name)
                            url = "https://i.4cdn.org/{}/{}{}".format(board.charname, post["tim"], post["ext"])
                            self._download(url, filepath)
                            # MAKE THUMBNAILS!
                            Webm.objects.create(filehash=md5,
                                filename=valuable_data["filename"],
                                filepath=filepath,
                                post=post_obj,
                                size=valuable_data["siz"],
                            )
