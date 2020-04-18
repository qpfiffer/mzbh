from mzbh.commands.downloader import downloader_4ch
from mzbh.commands.waifu_migrate import waifu_migrate
from mzbh.commands.spider import spider

def command_init(app):
    app.cli.add_command(downloader_4ch)
    app.cli.add_command(waifu_migrate)
    app.cli.add_command(spider)
