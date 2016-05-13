from django.conf import settings
from django.conf.urls import url
from django.conf.urls.static import static
from django.contrib import admin

from apps.main.views import home, board, webm

urlpatterns = [
    url(r'^$', home, name='home'),
    url(r'^chug/(?P<board_char>[a-zA-Z]+)/$', board, name='board'),
    url(r'^slurp/(?P<webm_id>[0-9]+)/$', webm, name='webm'),
    url(r'^admin/', admin.site.urls),
] + static(settings.STATIC_URL, document_root=settings.STATIC_URL)
