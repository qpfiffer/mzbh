from mzbh.cache import cache
from mzbh.models import Category

def _get_thumb_filename(filepath):
    split_path = filepath.split("/")
    filename = "thumb_{}.jpg".format(split_path[-1])
    dire = "/".join(filepath.split("/")[:-1]) + "/t/"
    return filename, dire

def thumbnail_filter(x):
    filename, _ = _get_thumb_filename(x.file_path)
    return filename

def id_to_board(x):
    val = cache.get("id_to_board")
    if val:
        return val[x]

    new_val = {}
    for cat in Category.query.all():
        new_val[cat.id] = cat.name

    cache.set("id_to_board", new_val)
    return new_val[x]

