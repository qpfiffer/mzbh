from flask import Blueprint, render_template

from mzbh.database import get_boards

blueprint = Blueprint('main', __name__)

@blueprint.route("/", methods=("GET",))
def root():
    return render_template("index.html")
