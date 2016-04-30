from django.conf import settings
from django.core.management.base import BaseCommand, CommandError
#from polls.models import Question as Poll

class Command(BaseCommand):
    help = 'Downloads porn from the internet.'

    def handle(self, *args, **options):
        all_boards = settings.ALL_BOARDS
        self.stdout.write("BOARDS: {}".format(all_boards))
        #for poll_id in options['poll_id']:
        #    try:
        #        poll = Poll.objects.get(pk=poll_id)
        #    except Poll.DoesNotExist:
        #        raise CommandError('Poll "%s" does not exist' % poll_id)

        #    poll.opened = False
        #    poll.save()

        #    self.stdout.write(self.style.SUCCESS('Successfully closed poll "%s"' % poll_id))
