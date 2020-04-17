import os
from flask import abort, Blueprint, g, render_template, send_file, safe_join

from mzbh.database import db
from mzbh.models import Category, Webm, WebmAlias, Post

from urllib.parse import unquote

blueprint = Blueprint('main', __name__)

@blueprint.route('/chug/<board>/t/<filename>')
def thumbnail_view(board, filename):
    dequoted = unquote(filename)
    path = safe_join('webms', board, 't', dequoted)
    if not os.path.exists(path):
        new_path = path.replace(".webm.jpg", ".jpg")
        if os.path.exists(new_path):
            # coimpatibility with old-style thumbnails.
            return send_file(new_path)
        return send_file("static/img/404.bmp")
    return send_file(path)

@blueprint.route("/chug/<board>", methods=("GET",))
def board(board):
    board = Category.query.filter_by(name=board).first()
    if board is None:
        abort(404)

    boards = Category.query.all()
    webms = Webm.query.filter_by(category_id=board.id)
    d = {
        "current_board": board.name,
        "boards": [x.name for x in boards],
        "webm_count": webms.count(),
        "alias_count": WebmAlias.query.filter_by(category_id=board.id).count(),
        "images": webms.all()[:100],
    }

    return render_template("board.html", **d)

@blueprint.route("/", methods=("GET",))
def root():
    boards = Category.query.all()
    d = {
        "boards": [x.name for x in boards],
        "webm_count": Webm.query.count(),
        "alias_count": WebmAlias.query.count(),
        "post_count": Post.query.count(),
        "categories": Category.query.all(),
    }

    return render_template("index.html", **d)
