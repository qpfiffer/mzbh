from django.shortcuts import render

from .models import Webm, WebmAlias, Post, Board

def home(request):
    webm_count = Webm.objects.all().count()
    webm_alias_count = WebmAlias.objects.all().count()
    post_count = Post.objects.all().count()
    return render(request, "main/home.html", locals())

def board(request, board_char):
    board = Board.objects.get(charname=board_char)
    images = Webm.objects.filter(post__thread__board=board)
    total_webms = images.count()
    total_webms += WebmAlias.objects.filter(post__thread__board=board).count()
    return render(request, "main/board.html", locals())

def webm(request, webm_id):
    image = Webm.objects.get(pk=webm_id)
    return render(request, "main/webm.html", locals())
