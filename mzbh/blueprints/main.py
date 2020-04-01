from flask import Blueprint, g, render_template

from mzbh.database import db
from mzbh.models import Category, Webm, WebmAlias, Post

blueprint = Blueprint('main', __name__)

@blueprint.route("/", methods=("GET",))
def root():
    d = {
        "webm_count": Webm.query.count(),
        "alias_count": WebmAlias.query.count(),
        "post_count": Post.query.count(),
        "categories": Category.query.all(),
    }

    return render_template("index.html", **d)
