#!/bin/bash

PREVIOUS=$(docker ps -a | grep 'waifu.xyz' | awk '{print $1}')
docker stop $PREVIOUS
docker rm $PREVIOUS

set -e
tar -czh Dockerfile src/ include/ templates/ static/ Makefile | docker build -t waifu.xyz -
docker run --rm -it -t \
    -e "WFU_WEBMS_DIR=/app/webms" \
    -v $(pwd)/webms:/app/webms -p 8080:8080 waifu.xyz
