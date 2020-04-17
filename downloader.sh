#!/usr/bin/env bash
while true; do
       FLASK_ENV=development FLASK_APP=mzbh flask downloader_4ch
       sleep 3600
done
