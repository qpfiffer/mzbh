# Introduction

This is a web-app built from the ground up with no external dependencies, aside
from [OlegDB](https://olegdb.org/), the database backend. It's job is pretty simple: Collect and host `.webm` files.
It was built with the following goals in mind:

* No dependencies (other than files I can embed into the project directly)
* Should be able to run indefinitely (no memory leaks)
* Should be pretty fast
* Should be able to run on a cheapo, $5.00 a month 32-bit VPS with no issues.

## Webserver

The webserver is something of my own design because I hate dependencies. It's
not great but it'll get the job done. You can run just the webserver with:

```
./waifu.xyz --serve
```

This runs the webserver by itself without starting the scraper component.

The webserver also accepts a thread counter argument, `-t`. This specifies the
number of threads in the acceptor pool.

```
./waifu.xyz -t 4
```

This runs the webserver with 4 threads. The default is 2.

## Scraper

The scraper hits the 4chan API very slowly and fetches threads it thinks will
have `.webm`s in them. It then downloads the `.webm` and thumbnails for
thumbnails for said `.webm`. These are stuck in the `WFU_WEBMS_DIR` location,
organized by board.

The goal is to retain as much metadata as possible while still allowing the
files to exist in a pure state (no hash filenames, not embedded in a database
somewhere.)

# Installation

1. `make`

There is no installation. this will just build a handful of binaries you can use.

* `waifu.xyz` - The main webserver/application and scraper. This is the meat of
  everything.
* `dbctl` - Handy cli program to manage and inspect and manage the DB state.
* `greshunkel_test` - Tests for the GRESHUNKEL templating language.
* `unit_test` - Random unit tests. Not really organized, mostly to prevent
  regressions.

# Running

## Web server

You can either run the main binary inside of Docker (`bootstrap.sh` is handy for
this), inside a Vagrant VM, or you can just run the binary raw. There are a handful
of environment variables you can set to affect where the program will keep files:

* `WFU_WEBMS_DIR` - Location to store webm files. Defaults to `./webms/`

## Database

You'll need to download and install [OlegDB](https://olegdb.org/). `waifu.xyz`
by default expects the database to be running on `localhost:38080`. Just make
sure this is the case.

## Running Raw

After compiling with `make`, just run the created binary:

```
./waifu.xyz
```

This will run the web app frontend along with the scraper.

## Running with Vagrant

You can install and run everything from a Vagrant VM. It is configured to share
and use the repo directory for data.

```Bash
vagrant up --provision
vagrant ssh -c 'cd /waifu.xyz && ./waifu.xyz'
```
