#!/bin/bash

echo "<!DOCTYPE html><html><body><ul>" > ./webms/index.html

for i in $(ls ./webms/ | grep -v index.html); do
    pushd "./webms/$i"
    for j in $(ls *.webm | sed 's/ /%20/g' ); do
        THUMB=$(echo -n "$j" | sed 's/ /%20/g' | sed 's/^/thumb_/g' | sed 's/.webm$/.jpg/g')
        echo "<li><a href="/$i/$j"><img src="/$i/$THUMB"></li>" >> ../index.html
    done
    popd
done

echo "</ul></body></html>" >> ./webms/index.html
