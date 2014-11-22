// vim: noet ts=4 sw=4
#pragma once

// Size of BMW hash
#define IMAGE_HASH_SIZE 256

int hash_image(const char *file_path, char outbuf[128]);
