from mzbh.database import db
from mzbh.filters import thumbnail_filter

def template_init(app):
    app.jinja_env.filters['thumbnail'] = thumbnail_filter

def get_or_create(model, **kwargs):
    instance = model.query.filter_by(**kwargs).first()
    if instance:
        return instance, False
    else:
        instance = model(**kwargs)
        db.session.add(instance)
        db.session.commit()
        return instance, True
