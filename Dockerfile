FROM phusion/baseimage

RUN apt-get update && apt-get install build-essential -y

RUN mkdir -p /app; mkdir -p /app/webms
COPY src /app/src
COPY include /app/include
COPY templates /app/templates
COPY static /app/static
COPY Makefile /app/Makefile

RUN cd /app; make
COPY run/waifu /etc/service/waifu/run

VOLUME /app/webms
EXPOSE 8080
CMD ["/sbin/my_init"]
