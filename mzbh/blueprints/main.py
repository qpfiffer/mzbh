from flask import Blueprint, g, render_template

from mzbh.database import db
from mzbh.models import Category

blueprint = Blueprint('main', __name__)

@blueprint.route("/", methods=("GET",))
def root():
    d = {
        "categories": Category.query.all(),
    }

    return render_template("index.html", **d)
