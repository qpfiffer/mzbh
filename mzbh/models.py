import sqlalchemy
from sqlalchemy.dialects.postgresql import UUID, JSONB

from mzbh.database import db


class Post(db.Model):
    __tablename__ = 'post'
    __table_args__ = { 'schema': 'mzbh' }

    id = db.Column(UUID(as_uuid=True), server_default=sqlalchemy.text("uuid_generate_v4()"), primary_key=True)
    old_id = db.Column(db.Integer)
    created_at = db.Column(db.DateTime)
    updated_at = db.Column(db.DateTime)

    posted_at = db.Column(db.DateTime)
    source_post_id = db.Column(db.BigInteger)
    oleg_key = db.Column(db.String)
    thread_id = db.Column(db.String)
    category_id = db.Column(db.String)
    replied_to_keys = db.Column(JSONB)
    body_content = db.Column(db.String)


class Thread(db.Model):
    __tablename__ = 'thread'
    __table_args__ = { 'schema': 'mzbh' }

    id = db.Column(UUID(as_uuid=True), server_default=sqlalchemy.text("uuid_generate_v4()"), primary_key=True)
    old_id = db.Column(db.Integer)
    created_at = db.Column(db.DateTime)
    updated_at = db.Column(db.DateTime)

    oleg_key = db.Column(db.String)
    category_id = db.Column(db.String)
    subject = db.Column(db.String)
    thread_ident = db.Column(db.BigInteger)


class Category(db.Model):
    """
    This is a board on 4chan, subreddit on reddit, etc.
    """
    __tablename__ = 'category'
    __table_args__ = { 'schema': 'mzbh' }

    id = db.Column(UUID(as_uuid=True), server_default=sqlalchemy.text("uuid_generate_v4()"), primary_key=True)
    created_at = db.Column(db.DateTime)
    updated_at = db.Column(db.DateTime)

    name = db.Column(db.String, nullable=False)
    host_id = db.Column(db.String)


class Host(db.Model):
    __tablename__ = 'host'
    __table_args__ = { 'schema': 'mzbh' }

    id = db.Column(UUID(as_uuid=True), server_default=sqlalchemy.text("uuid_generate_v4()"), primary_key=True)
    created_at = db.Column(db.DateTime)
    updated_at = db.Column(db.DateTime)

    name = db.Column(db.String, nullable=False)


class User(db.Model):
    __tablename__ = 'user'
    __table_args__ = { 'schema': 'mzbh' }

    id = db.Column(UUID(as_uuid=True), server_default=sqlalchemy.text("uuid_generate_v4()"), primary_key=True)
    created_at = db.Column(db.DateTime)
    updated_at = db.Column(db.DateTime)
    email_address = db.Column(db.String)
    password = db.Column(db.String)


class Webm(db.Model):
    __tablename__ = 'webm'
    __table_args__ = { 'schema': 'mzbh' }

    id = db.Column(UUID(as_uuid=True), server_default=sqlalchemy.text("uuid_generate_v4()"), primary_key=True)
    old_id = db.Column(db.Integer)
    created_at = db.Column(db.DateTime)
    updated_at = db.Column(db.DateTime)

    oleg_key = db.Column(db.String)
    file_hash = db.Column(db.String, nullable=False)
    filename = db.Column(db.String)
    category_id = db.Column(db.String)
    file_path = db.Column(db.String)
    post_id = db.Column(db.String)
    size = db.Column(db.Integer)


class WebmAlias(db.Model):
    __tablename__ = 'webm_alias'
    __table_args__ = { 'schema': 'mzbh' }

    id = db.Column(UUID(as_uuid=True), server_default=sqlalchemy.text("uuid_generate_v4()"), primary_key=True)
    old_id = db.Column(db.Integer)
    created_at = db.Column(db.DateTime)
    updated_at = db.Column(db.DateTime)

    oleg_key = db.Column(db.String)
    file_hash = db.Column(db.String, nullable=False)
    filename = db.Column(db.String)
    category_id = db.Column(db.String)
    file_path = db.Column(db.String)
    post_id = db.Column(db.String)
    webm_id = db.Column(db.String)
