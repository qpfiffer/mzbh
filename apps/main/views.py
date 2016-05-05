from django.shortcuts import render

from .models import Webm, WebmAlias, Post

def home(request):
    webm_count = Webm.objects.all().count()
    webm_alias_count = WebmAlias.objects.all().count()
    post_count = Post.objects.all().count()
    return render(request, "main/home.html", locals())
