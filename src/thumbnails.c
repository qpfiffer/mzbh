// vim: noet ts=4 sw=4
// From here, basically: https://ffmpeg.org/doxygen/0.6/output-example_8c-source.html
#include <libavformat/avformat.h>

#include <38-moths/38-moths.h>

#include "thumbnails.h"
#include "utils.h"

int generate_thumbnail(const char webm_path[static MAX_IMAGE_FILENAME_SIZE],
					   const char thumbnail_path[static MAX_IMAGE_FILENAME_SIZE]) {
	AVOutputFormat *fmt = av_guess_format("jpeg", NULL, NULL);;
	AVFormatContext *oc = NULL;
	AVStream *video_st = NULL;

	av_register_all();

	oc = avformat_alloc_context();
	if (!oc) {
		m38_log_msg(LOG_ERR, "Could not allocate AVFormat.");
		return 1;
	}
	oc->oformat = fmt;
	strncpy(oc->filename, webm_path, sizeof(oc->filename));

	video_st = add_video_stream(oc, fmt->video_codec);

	if (av_set_parameters(oc, NULL) < 0) {
		m38_log_msg(LOG_ERR, "Invalid output parameters.");
		return 1;
	}

	dump_format(oc, 0, webm_path, 1);

	open_video(oc, video_st);
	if (url_fopen(&oc->pb, webm_path, URL_WRONLY) < 0) {
		m38_log_msg(LOG_ERR, "Could not open '%s'", webm_path);
		return 1;
	}

	UNUSED(thumbnail_path);

	return 0;
}

