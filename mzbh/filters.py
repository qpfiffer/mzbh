def _get_thumb_filename(filepath):
    split_path = filepath.split("/")
    filename = "thumb_{}.jpg".format(split_path[-1])
    dire = "/".join(filepath.split("/")[:-1]) + "/t/"
    return filename, dire

def thumbnail_filter(x):
    filename, _ = _get_thumb_filename(x.file_path)
    return filename
