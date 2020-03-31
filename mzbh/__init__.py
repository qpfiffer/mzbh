from flask import Flask
from flask_sqlalchemy import SQLAlchemy

from mzbh import settings

def create_app():
    from mzbh.blueprints import main

    app = Flask(__name__, instance_relative_config=True)
    app.config.from_mapping(
        SECRET_KEY=settings.SECRET_KEY,
        SQLALCHEMY_DATABASE_URI=settings.SQLALCHEMY_DATABASE_URI,
    )

    db = SQLAlchemy(app)

    app.register_blueprint(main.blueprint)

    return app
