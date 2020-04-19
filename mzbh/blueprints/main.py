import os
from flask import abort, Blueprint, g, render_template, send_file, safe_join
from flask_caching import Cache

from mzbh.cache import cache
from mzbh.database import db
from mzbh.models import Category, Webm, WebmAlias, Post, Thread

from urllib.parse import unquote

blueprint = Blueprint('main', __name__)

IMAGE_COUNT = 100

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

@blueprint.route('/slomp/<webm_id>')
def slomp_view(webm_id):
    webm = Webm.query.filter_by(id=webm_id).first()
    return send_file(webm.file_path)

def _paginated(page, pages, current_board, webm_count, webms, webm_alias_count, template="board.html"):
    boards = Category.query.order_by(Category.name).all()

    d = {
        "prev_page": page - 1 if page - 1 > 0 else 0,
        "next_page": page + 1 if page + 1 < pages else pages,
        "pages": range(0, pages),
        "current_board": current_board,
        "boards": [x.name for x in boards],
        "webm_count": webm_count,
        "alias_count": webm_alias_count,
        "images": webms.offset(IMAGE_COUNT * page).limit(IMAGE_COUNT),
    }

    return render_template(template, **d)

@blueprint.route("/by/alias/<page>", methods=("GET",))
def paginated_by_alias(page):
    webms = Webm.query\
            .outerjoin(WebmAlias)\
            .group_by(Webm.id)\
            .order_by(db.func.count(WebmAlias.id).desc(), Webm.created_at.desc())
    webm_count = webms.count()
    webm_alias_count = WebmAlias.query.count()
    page = int(page)
    pages = int(webm_count / IMAGE_COUNT)

    return _paginated(page, pages, "xxx", webm_count, webms, webm_alias_count, template="aliased.html")

@blueprint.route("/by/alias/<page>", methods=("GET",))
def by_alias(board, page):
    return paginated_by_newest(0)

@blueprint.route("/by/newest/<page>", methods=("GET",))
def paginated_by_newest(page):
    webms = Webm.query.order_by(Webm.created_at.desc())
    webm_count = webms.count()
    webm_alias_count = WebmAlias.query.count()
    page = int(page)
    pages = int(webm_count / IMAGE_COUNT)

    return _paginated(page, pages, "xxx", webm_count, webms, webm_alias_count, template="newest.html")

@blueprint.route("/by/newest/<page>", methods=("GET",))
def by_newest(board, page):
    return paginated_by_newest(0)

@blueprint.route("/chug/<board>/<page>", methods=("GET",))
def paginated_board(board, page):
    board = Category.query.filter_by(name=board).first()
    if board is None:
        abort(404)

    webms = Webm.query.order_by(Webm.created_at.desc()).filter_by(category_id=board.id)
    webm_count = webms.count()
    webm_alias_count = WebmAlias.query.filter_by(category_id=board.id).count()
    page = int(page)
    pages = int(webm_count / IMAGE_COUNT)

    return _paginated(page, pages, board.name, webm_count, webms, webm_alias_count)

@blueprint.route("/chug/<board>", methods=("GET",))
def board(board):
    return paginated_board(board, 0)

@blueprint.route("/slurp/<image_id>", methods=("GET",))
def webm_view(image_id):
    boards = Category.query.order_by(Category.name).all()
    webm = Webm.query.filter_by(id=image_id).first()
    aliases = WebmAlias.query.filter_by(webm_id=webm.id).all()
    post = Post.query.filter_by(id=webm.post_id).first()
    category = Category.query.filter_by(id=webm.category_id).first()
    thread = None
    if post:
        thread = Thread.query.filter_by(id=post.thread_id).first()

    d = {
        "aliases": aliases,
        "boards": [x.name for x in boards],
        "webm": webm,
        "category": category,
        "post": post,
        "thread": thread,
    }
    return render_template("webm.html", **d)

@blueprint.route("/", methods=("GET",))
def root():
    boards = Category.query.order_by(Category.name).all()
    d = {
        "boards": [x.name for x in boards],
        "webm_count": Webm.query.count(),
        "alias_count": WebmAlias.query.count(),
        "post_count": Post.query.count(),
        "categories": Category.query.all(),
    }

    return render_template("index.html", **d)
