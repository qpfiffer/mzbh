// vim: noet ts=4 sw=4
#include <libavformat/avformat.h>

#include "thumbnails.h"
#include "utils.h"

int generate_thumbnail(const char webm_path[static MAX_IMAGE_FILENAME_SIZE],
					   const char thumbnail_path[static MAX_IMAGE_FILENAME_SIZE]) {
	AVOutputFormat *fmt = NULL;
	AVFormatContext *oc = NULL;
	AVStream *video_st = NULL;

	av_register_all();

	UNUSED(webm_path);
	UNUSED(thumbnail_path);
	UNUSED(fmt);
	UNUSED(oc);
	UNUSED(video_st);

	return 0;
}

