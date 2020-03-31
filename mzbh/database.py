from flask_sqlalchemy import SQLAlchemy
from werkzeug.local import LocalProxy

from mzbh.settings import DATABASE_URI

db = SQLAlchemy()

def init(app):
    app.config['SQLALCHEMY_DATABASE_URI'] = DATABASE_URI
    db.init_app(app)
