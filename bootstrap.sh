#!/bin/bash -e

PREVIOUS=$(docker ps -a | grep 'waifu.xyz' | awk '{print $1}')
docker stop $PREVIOUS
docker rm $PREVIOUS

tar -czh Dockerfile src/ include/ Makefile | docker build -t waifu.xyz -
docker run --rm -t -v $(pwd)/webms:/app/webms -p 8080:8080 waifu.xyz
