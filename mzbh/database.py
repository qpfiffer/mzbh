from flask import g, current_app
from sqlalchemy import create_engine
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import scoped_session, sessionmaker
from werkzeug.local import LocalProxy

from mzbh.settings import DATABASE_URI

_engine = create_engine(DATABASE_URI)
engine = LocalProxy(lambda: _engine)

_base = declarative_base(engine)
Base = LocalProxy(lambda: _base)

_session = scoped_session(sessionmaker(bind=engine))
Session = LocalProxy(lambda: _session)

def _get_db():
    g.db = engine.connect()
    return g.db

def _get_session():
    g.db_session = Session()
    return g.db_session

def _close_session(app):
    db_session = g.pop('db_session', None)
    if db_session is not None:
        db_session.close()

def _close_db(app):
    db = g.pop('db', None)
    if db is not None:
        db.close()

def init():
    db = _get_db()
    session = _get_session()
    current_app.teardown_appcontext(_close_session)
    current_app.teardown_appcontext(_close_db)
