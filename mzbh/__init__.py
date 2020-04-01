from flask import Flask

from mzbh.commands.downloader import downloader_4ch
from mzbh import settings


def create_app():
    from mzbh.blueprints import main
    from mzbh.database import init as db_init

    app = Flask(__name__, instance_relative_config=True)
    app.config.from_mapping(
        SECRET_KEY=settings.SECRET_KEY,
    )

    app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

    app.register_blueprint(main.blueprint)

    db_init(app)

    @app.cli.command("downloader_4ch")
    def f():
        return downloader_4ch()

    return app
