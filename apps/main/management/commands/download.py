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

    def handle(self, *args, **options):
        self._ensure_dir(WEBMS_DIR)
        for board in Board.objects.all():
            thread_nums_with_maybe_matches = self._create_index_for_catalog(board)

            to_download = []
            for t_num in thread_nums_with_maybe_matches:
                self._ensure_dir(os.path.join(WEBMS_DIR, board.charname))
                thread_data = requests.get("https://a.4cdn.org/{}/thread/{}.json".format(board.charname, t_num))
                thread_data = thread_data.json()
                for post in thread_data["posts"]:
                    valuable_data = {
                        "file_ext": post.get("ext", None),
                        "filename": post.get("filename", ""),
                        "no": post["no"],
                        "siz": post.get("fsize", 0),
                        "_tim": post.get("tim", None),
                        "body_content": post.get("com", ""),
                        "should_download_image": False
                    }

                    if valuable_data["file_ext"] is None:
                        continue

                    if valuable_data["file_ext"] == ".webm":
                        valuable_data["should_download_image"] = True

                    to_download.append(valuable_data)

            import ipdb; ipdb.set_trace()
            "Download stuff here."
