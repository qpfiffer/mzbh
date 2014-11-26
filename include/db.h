// vim: noet ts=4 sw=4
#pragma once

// Size of BMW hash
#define IMAGE_HASH_SIZE 1024
#define HASH_ARRAY_SIZE IMAGE_HASH_SIZE/8

/* Namespaces for when we create our keys */
#define WAIFU_NMSPC waifu
#define WEBM_NMSPC webm

int hash_image(const char *file_path, char outbuf[128]);
