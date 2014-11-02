
```Bash
docker build -t waifu.xyz .
docker run -t -v $(pwd)/webms:/app/webms -p 8080:8080 waifu.xyz
````
