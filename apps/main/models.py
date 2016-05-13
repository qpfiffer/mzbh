from __future__ import unicode_literals

from django.db import models

class Board(models.Model):
    fullname = models.TextField(max_length=32)
    charname = models.CharField(max_length=16)

class Thread(models.Model):
    board = models.ForeignKey(Board)
    created_at = models.DateTimeField(auto_now=True)
    thread_ident = models.CharField(max_length=32, null=False)

class Post(models.Model):
    # Post Date:
    post_id = models.CharField(max_length=32)
    post_no = models.CharField(max_length=32)
    thread = models.ForeignKey(Thread)
    body_content = models.TextField(null=True, blank=True)
    created_at = models.DateTimeField(auto_now=True)

class Webm(models.Model):
    filehash = models.CharField(max_length=256)
    filename = models.TextField(null=False, blank=False)
    filepath = models.TextField(null=False, blank=False)
    post = models.ForeignKey(Post)
    size = models.BigIntegerField(default=0)
    created_at = models.DateTimeField(auto_now=True)

class WebmAlias(models.Model):
    """
    This is an alias (copy of) another webm. We don't actually
    have this file anywhere but we've seen it and recorded that we
    saw it.
    """
    filename = models.TextField(null=False, blank=False)
    post = models.ForeignKey(Post)
    created_at = models.DateTimeField(auto_now=True)
    webm = models.ForeignKey(Webm)
