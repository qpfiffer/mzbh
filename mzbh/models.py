from mzbh.database import db


class Category(db.Model):
    __tablename__ = 'category'
    __table_args__ = { 'schema': 'mzbh' }

    id = db.Column(db.String, primary_key=True)
    created_at = db.Column(db.DateTime)
    updated_at = db.Column(db.DateTime)

    name = db.Column(db.String, nullable=False)
    host_id = db.Column(db.String)


class Host(db.Model):
    __tablename__ = 'host'
    __table_args__ = { 'schema': 'mzbh' }

    id = db.Column(db.String, primary_key=True)
    created_at = db.Column(db.DateTime)
    updated_at = db.Column(db.DateTime)

    name = db.Column(db.String, nullable=False)


class User(db.Model):
    __tablename__ = 'user'
    __table_args__ = { 'schema': 'mzbh' }

    id = db.Column(db.String, primary_key=True)
    created_at = db.Column(db.DateTime)
    updated_at = db.Column(db.DateTime)
    email_address = db.Column(db.String)
    password = db.Column(db.String)
