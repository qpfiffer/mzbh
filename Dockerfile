FROM mwcampbell/muslbase

RUN mkdir -p /app; mkdir -p /app/webms
COPY src /app/src
COPY include /app/include
COPY Makefile /app/Makefile

RUN cd /app; make

VOLUME /app/webms
WORKDIR /app
EXPOSE 8080
CMD ["./waifu.xyz"]
