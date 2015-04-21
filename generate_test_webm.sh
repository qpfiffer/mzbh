#!/bin/bash

avconv -t 10 -s 640x480 -f rawvideo -pix_fmt rgb24 -r 25 -i /dev/zero -t 10 silence.webm
avconv -i silence.webm -vframes 1 thumb_silence.jpg
