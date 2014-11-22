# Introduction

This is a web-app built from the ground up with no external dependencies. It's
job is pretty simple: Collect and host `.webm` files. It was built with the
following goals in mind:

* No dependencies (other than files I can embed into the project directly)
* Should be able to run indefinitely (no memory leaks)
* Should be pretty fast
* Should be able to run on a cheapo, $5.00 a month 32-bit VPS with no issues.

# Installation

1. `make`

There is no installation. this will just build a handful of binaries you can use.

* `waifu.xyz` - The main webserver/application and scraper. This is the meat of
  everything.
* `dbctl` - Handy cli program to manage and inspect the DB state.
* `greshunkel_test` - Tests for the GRESHUNKEL templating language.

# Running

You can either run the main binary inside of Docker (`bootstrap.sh` is handy for
this) or you can just run the binary raw. There are a handful of environment
variables you can set to affect where the program will keep files:

* `WFU_WEBMS_DIR` - Location to store webm files. Defaults to `./webms/`
* `WFU_DB_LOCATION` - Location to store the main database file. Defaults to
  `./webms/waifu.db`

## Running with Docker

The `bootstrap.sh` script basically does this:

```Bash
docker build -t waifu.xyz .
docker run --rm -t -v $(pwd)/webms:/app/webms -p 8080:8080 waifu.xyz
````
