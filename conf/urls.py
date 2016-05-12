from django.conf import settings
from django.conf.urls import url
from django.conf.urls.static import static
from django.contrib import admin

from apps.main.views import home, board

urlpatterns = [
    url(r'^$', home, name='home'),
    url(r'^chug/(?P<board>[a-zA-Z]+)/$', board, name='board'),
    url(r'^admin/', admin.site.urls),
] + static(settings.STATIC_URL, document_root=settings.STATIC_URL)
