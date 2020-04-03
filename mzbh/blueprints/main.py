from flask import abort, Blueprint, g, render_template

from mzbh.database import db
from mzbh.models import Category, Webm, WebmAlias, Post

blueprint = Blueprint('main', __name__)

@blueprint.route("/chug/<board>", methods=("GET",))
def board(board):
    board = Category.query.filter_by(name=board).first()
    if board is None:
        abort(404)

    boards = Category.query.all()
    d = {
        "current_board": board.name,
        "boards": [x.name for x in boards],
        "webm_count": Webm.query.filter_by(category_id=board.id).count(),
        "alias_count": WebmAlias.query.filter_by(category_id=board.id).count(),
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
