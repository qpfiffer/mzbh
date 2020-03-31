from flask import Blueprint, g, render_template

from mzbh.models import Category

blueprint = Blueprint('main', __name__)

@blueprint.route("/", methods=("GET",))
def root():
    session = g.db_session
    d = {
        "categories": session.query(Category).all(),
    }

    return render_template("index.html", **d)
