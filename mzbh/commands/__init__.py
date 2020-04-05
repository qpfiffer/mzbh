from mzbh.commands.downloader import downloader_4ch

def command_init(app):
    app.cli.add_command(downloader_4ch, name="downloader_4ch")
