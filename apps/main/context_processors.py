
from .models import Board

def all_boards(request):
    ct = { 'all_boards': [x.name for x in Board.objects.all()]}
    return ct
