from sqlalchemy import Column, DateTime, String
from sqlalchemy.ext.declarative import declarative_base

Base = declarative_base()


class Category(Base):
    __tablename__ = 'mzbh.category'

    id = Column(String, primary_key=True)
    created_at = Column(DateTime)
    updated_at = Column(DateTime)

    name = Column(String, nullable=False)
    host_id = Column(String)


class Host(Base):
    __tablename__ = 'mzbh.host'

    id = Column(String, primary_key=True)
    created_at = Column(DateTime)
    updated_at = Column(DateTime)

    name = Column(String, nullable=False)


class User(Base):
    __tablename__ = 'mzbh.user'

    id = Column(String, primary_key=True)
    created_at = Column(DateTime)
    updated_at = Column(DateTime)
    email_address = Column(String)
    password = Column(String)
