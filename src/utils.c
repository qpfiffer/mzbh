// vim: noet ts=4 sw=4
#ifdef __clang__
	#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <38-moths/logging.h>

#include "models.h"
#include "parse.h"
#include "sha3api_ref.h"
#include "utils.h"

/* Kooky struct with a bunch of data in it. */
typedef struct AVContext {
	AVFormatContext *fmt_ctx;
	AVCodecContext * video_dec_ctx;
	int video_stream_idx;
	AVFrame *frame;
	AVPacket pkt;
	AVCodec *dec;
} AVContext;

const char WEBMS_DIR_DEFAULT[] = "./webms";
const char *WEBMS_DIR = NULL;

const char DB_HOST_DEFAULT[] = "127.0.0.1";
const char *DB_HOST = NULL;

const int DB_PORT_DEFAULT = 38080;
const int DB_PORT = 0;

const char *webm_location() {
	if (!WEBMS_DIR) {
		char *env_var = getenv("WFU_WEBMS_DIR");
		if (!env_var) {
			WEBMS_DIR = WEBMS_DIR_DEFAULT;
		} else {
			WEBMS_DIR = env_var;
		}
	}

	return WEBMS_DIR;
}

void ensure_directory_for_board(const char *board) {
	/* Long enough for WEBMS_DIR, a /, the board and a NULL terminator */
	const size_t buf_siz = strlen(webm_location()) + sizeof(char) * 2 + strnlen(board, MAX_BOARD_NAME_SIZE);
	char to_create[buf_siz];
	memset(to_create, '\0', buf_siz);

	/* ./webms/b */
	snprintf(to_create, buf_siz, "%s/%s", webm_location(), board);

	struct stat st = {0};
	if (stat(to_create, &st) == -1) {
		log_msg(LOG_WARN, "Creating directory %s.", to_create);
		mkdir(to_create, 0755);
	}
}


inline void get_non_colliding_image_filename(char fname[static MAX_IMAGE_FILENAME_SIZE], const post_match *p_match) {
	snprintf(fname, MAX_IMAGE_FILENAME_SIZE, "%zu_%s%.*s",
			p_match->size, p_match->filename, (int)sizeof(p_match->file_ext),
			p_match->file_ext);
}

int get_non_colliding_image_file_path(char fname[static MAX_IMAGE_FILENAME_SIZE], const post_match *p_match) {
	char _real_fname[MAX_IMAGE_FILENAME_SIZE] = {0};
	get_non_colliding_image_filename(_real_fname, p_match);

	snprintf(fname, MAX_IMAGE_FILENAME_SIZE, "%s/%s/%s", webm_location(), p_match->board, _real_fname);

	size_t fsize = get_file_size(fname);
	if (fsize == 0) {
		return 0;
	} else if (fsize == p_match->size) {
		log_msg(LOG_INFO, "Skipping %s.", fname);
		return 1;
	} else if (fsize != p_match->size) {
		log_msg(LOG_WARN, "Found duplicate filename for %s with incorrect size. Bad download?",
				fname);
		return 0;
	}

	return 0;
}

void ensure_thumb_directory(const post_match *p_match) {
	char thumb_dir[MAX_IMAGE_FILENAME_SIZE] = {0};
	snprintf(thumb_dir, MAX_IMAGE_FILENAME_SIZE, "%s/%s/t", webm_location(), p_match->board);

	struct stat st = {0};
	if (stat(thumb_dir, &st) == -1) {
		log_msg(LOG_WARN, "Creating thumb directory %s.", thumb_dir);
		mkdir(thumb_dir, 0755);
	}
}

void get_thumb_filename(char thumb_filename[static MAX_IMAGE_FILENAME_SIZE], const post_match *p_match) {
	snprintf(thumb_filename, MAX_IMAGE_FILENAME_SIZE, "%s/%s/t/thumb_%zu_%s.jpg",
			webm_location(), p_match->board, p_match->size, p_match->filename);
}

int endswith(const char *string, const char *suffix) {
	size_t string_siz = strlen(string);
	size_t suffix_siz = strlen(suffix);

	if (string_siz < suffix_siz)
		return 0;

	unsigned int i = 0;
	for (; i < suffix_siz; i++) {
		if (suffix[i] != string[string_siz - suffix_siz + i])
			return 0;
	}
	return 1;
}

/* Pulled from here: http://stackoverflow.com/a/25705264 */
char *strnstr(const char *haystack, const char *needle, size_t len) {
	int i;
	size_t needle_len;

	/* segfault here if needle is not NULL terminated */
	if (0 == (needle_len = strlen(needle)))
		return (char *)haystack;

	/* Limit the search if haystack is shorter than 'len' */
	len = strnlen(haystack, len);

	for (i=0; i<(int)(len-needle_len); i++)
	{
		if ((haystack[0] == needle[0]) && (0 == strncmp(haystack, needle, needle_len)))
			return (char *)haystack;

		haystack++;
	}
	return NULL;
}

void url_decode(const char *src, const size_t src_siz, char *dest) {
	unsigned int srcIter = 0, destIter = 0;
	char to_conv[] = "00";

	while (srcIter < src_siz) {
		if (src[srcIter] == '%' && srcIter + 2 < src_siz
				&& isxdigit(src[srcIter + 1]) && isxdigit(src[srcIter + 2])) {
			/* Theres definitely a better way to do this but I don't care
			 * right now. */
			to_conv[0] = src[srcIter + 1];
			to_conv[1] = src[srcIter + 2];

			long int converted = strtol(to_conv, NULL, 16);
			dest[destIter] = converted;

			srcIter += 3;
			destIter++;
		}

		dest[destIter] = src[srcIter];
		destIter++;
		srcIter++;
	}
}

time_t get_file_creation_date(const char *file_path) {
	struct stat st = {0};
	if (stat(file_path, &st) == -1)
		return 0;
	return st.st_mtime;
}

size_t get_file_size(const char *file_path) {
	struct stat st = {0};
	if (stat(file_path, &st) == -1)
		return 0;
	return st.st_size;
}

int hash_string(const unsigned char *string, const size_t siz, char outbuf[static HASH_IMAGE_STR_SIZE]) {
	unsigned char hash[HASH_ARRAY_SIZE] = {0};

	if (Hash(IMAGE_HASH_SIZE, string, siz, hash) != 0)
		return 0;

	int j = 0;
	for (j = 0; j < HASH_ARRAY_SIZE; j++)
		sprintf(outbuf + (j * 2), "%02X", hash[j]);

	return 1;
}

int hash_file(const char *file_path, char outbuf[static HASH_IMAGE_STR_SIZE]) {
	unsigned char *data_ptr = NULL;
	int fd = open(file_path, O_RDONLY);
	if (fd < 0) {
		log_msg(LOG_ERR, "Could not open file for hashing.");
		perror("hash_file");
		goto error;
	}

	struct stat st = {0};
	if (stat(file_path, &st) == -1)
		goto error;

	data_ptr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	int rc = hash_string(data_ptr, st.st_size, outbuf);

	munmap(data_ptr, st.st_size);
	close(fd);

	return rc;

error:
	if (data_ptr != NULL)
		munmap(data_ptr, st.st_size);
	close(fd);
	errno = 0;
	return 0;
}

int hash_string_fnv1a(const unsigned char *key, const size_t siz, char outbuf[static HASH_IMAGE_STR_SIZE]) {
	/* https://en.wikipedia.org/wiki/Fowler_Noll_Vo_hash */
	const uint64_t fnv_prime = 1099511628211ULL;
	const uint64_t fnv_offset_bias = 14695981039346656037ULL;

	const int iterations = siz;

	uint8_t i;
	uint64_t hash = fnv_offset_bias;

	for(i = 0; i < iterations; i++) {
		hash = hash ^ key[i];
		hash = hash * fnv_prime;
	}

	sprintf(outbuf, "%"PRIX64, hash);
	return 1;
}

inline char *get_full_path_for_file(const char *dir, const char file_name[static MAX_IMAGE_FILENAME_SIZE]) {
	const size_t siz = strlen(dir) + strlen("/") + strlen(file_name) + 1;
	char *fpath = malloc(siz);

	snprintf(fpath, siz, "%s/%s", dir, file_name);

	return fpath;
}

char *get_full_path_for_webm(const char current_board[MAX_BOARD_NAME_SIZE],
							 const char file_name_decoded[MAX_IMAGE_FILENAME_SIZE]) {
	const char *webm_loc = webm_location();
	const size_t full_path_size = strlen(webm_loc) + strlen("/") +
								  strlen(current_board) + strlen("/") +
								  strlen(file_name_decoded) + 1;

	char *full_path = malloc(full_path_size);
	memset(full_path, '\0', full_path_size);
	snprintf(full_path, full_path_size, "%s/%s/%s", webm_loc, current_board, file_name_decoded);

	return full_path;
}

static void _ensure_user_uploads_thumbs_dir() {
	struct stat st = {0};
	char buf[256] = {0};
	snprintf(buf, sizeof(buf), "%s/t", USER_UPLOADS_DIR);
	if (stat(USER_UPLOADS_DIR, &st) == -1) {
		log_msg(LOG_WARN, "Creating directory %s.", USER_UPLOADS_DIR);
		mkdir(USER_UPLOADS_DIR, 0755);
	}

	if (stat(buf, &st) == -1) {
		log_msg(LOG_WARN, "Creating directory %s.", buf);
		mkdir(buf, 0755);
	}
}

static int dump_frame_to_jpeg(const AVContext *av, const char *thumbnail_filename) {
	AVCodec *jpeg_codec = avcodec_find_encoder(AV_CODEC_ID_JPEG2000);
	if (!jpeg_codec)
		return -1;

	AVCodecContext *jpeg_ctext = avcodec_alloc_context3(jpeg_codec);
	if (!jpeg_ctext)
		return -1;

	jpeg_ctext->pix_fmt = av->video_dec_ctx->pix_fmt;
	jpeg_ctext->height = av->frame->height;
	jpeg_ctext->width = av->frame->width;
	jpeg_ctext->sample_aspect_ratio = av->video_dec_ctx->sample_aspect_ratio;
	jpeg_ctext->time_base = av->video_dec_ctx->time_base;
	jpeg_ctext->compression_level = 100;
	jpeg_ctext->thread_count = 1;
	jpeg_ctext->prediction_method = 1;
	jpeg_ctext->flags2 = 0;
	jpeg_ctext->rc_max_rate = jpeg_ctext->rc_min_rate = jpeg_ctext->bit_rate = 80000000;

	if (avcodec_open2(jpeg_ctext, jpeg_codec, NULL) < 0)
		return -1;

	int found_frame = 0;
	AVPacket packet = {0};

	av_init_packet(&packet);
	/* av_dump_format(av->fmt_ctx, 0, "", 0); */

	if (avcodec_encode_video2(jpeg_ctext, &packet, av->frame, &found_frame) < 0)
		return -1;


	FILE *JPEGFile = fopen(thumbnail_filename, "wb");
	fwrite(packet.data, 1, packet.size, JPEGFile);
	fclose(JPEGFile);

	av_free_packet(&packet);
	avcodec_close(jpeg_ctext);

	return 0;
}

size_t create_thumbnail_for_webm(const char webm_file_path[static MAX_IMAGE_FILENAME_SIZE],
								 const char webm_filename[static MAX_IMAGE_FILENAME_SIZE],
								 char out_filepath[static MAX_IMAGE_FILENAME_SIZE]) {
	FILE *thumbnail_handle = NULL;
	size_t written = 0;
	AVFormatContext *format_ctext = NULL;
	int video_stream = -1;
	AVCodec *codec = NULL;
	AVCodecContext *codec_ctext = NULL;
	AVFrame *frame = NULL;
	AVFrame *frame_rgb = NULL;
	AVPacket packet = {0};
	int frame_finished = 0;
	size_t num_bytes = 0;
	uint8_t *buffer = NULL;

	_ensure_user_uploads_thumbs_dir();

	snprintf(out_filepath, MAX_IMAGE_FILENAME_SIZE, "%s/t/%s_thumb.jpg", USER_UPLOADS_DIR, webm_filename);

	av_register_all();

	if (av_open_input_file(&format_ctext, webm_file_path, NULL, 0, NULL) != 0) {
		log_msg(LOG_ERR, "Thumbnailer: Could not open user uploaded webm for thumbnailing. File: %s", webm_file_path);
		goto error;
	}

	if (av_find_stream_info(format_ctext) < 0) {
		log_msg(LOG_ERR, "Thumbnailer: Could not find stream information");
		goto error;
	}

	// Find the first video stream
	int i = 0;
	for (;i < format_ctext->nb_streams; i++) {
		if (format_ctext->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
			video_stream = i;
			break;
		}
	}

	if (video_stream == -1) {
		log_msg(LOG_ERR, "Thumbnailer: WEBM has no video streams.");
		goto error;
	}

	codec_ctext = format_ctx->streams[videoStream]->codec;

	codec = avcodec_find_decoder(codec_ctext->codec_id);
	if (codec == NULL) {
		log_msg(LOG_ERR, "Thumbnailer: Could not find codec.");
		goto error;
	}

	if (avcodec_open(codec_ctext, codec) < 0) {
		log_msg(LOG_ERR, "Thumbnailer: Could not open codec.");
		goto error;
	}

	frame = avcodec_alloc_frame();
	frame_rgb = avcodec_alloc_frame();

	if (frame_rgb == NULL) {
		log_msg(LOG_ERR, "Thumbnailer: Could not allocate frame_rgb.");
		goto error;
	}

	num_bytes = avpicture_get_size(PIX_FMT_RGB24, codec_ctext->width, codec_ctext->height);

	buffer = malloc(num_bytes);
	if (buffer == NULL) {
		log_msg(LOG_ERR, "Thumbnailer: Could not allocate space for pixel buffer..");
		goto error;
	}

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	avpicture_fill((AVPicture *)frame_rgb, buffer,
				   PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height);

	i = 0;
	while (av_read_frame(format_ctext, &packet) >= 0) {
		if (packet.stream_index == video_stream) {
			avcodec_decode_video(codec_ctext, frame, &frame_finished,
								 packet.data, packet.size);

			if (frameFinished) {
				AVContext av = {
					.fmt_ctx = format_ctext,
					.video_dec_ctx = codec_ctext,
					.frame = frame,
					.pkg = packet,
					.dec = codec
				};
				dump_frame_to_jpeg(&av, out_filepath);
				/* static struct SwsContext *img_convert_ctx;

				// Convert the image into YUV format that SDL uses
				if(img_convert_ctx == NULL) {
					int w = pCodecCtx->width;
					int h = pCodecCtx->height;
					
					img_convert_ctx = sws_getContext(w, h, 
									pCodecCtx->pix_fmt, 
									w, h, PIX_FMT_RGB24, SWS_BICUBIC,
									NULL, NULL, NULL);
					if(img_convert_ctx == NULL) {
						fprintf(stderr, "Cannot initialize the conversion context!\n");
						exit(1);
					}
				}
				int ret = sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0, 
						  pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
				if(i++<=5)
					SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
				*/
			}
		}

		av_free_packet(&packet);
	}

	free(buffer);
	av_free(frame_rgb);
	av_free(frame);
	avcodec_close(codec_ctext);

	av_close_input_file(format_ctext);

	return 1;

error:
	free(buffer);
	av_free(frame_rgb);
	av_free(frame);
	avcodec_close(codec_ctext);

	av_close_input_file(format_ctext);
	return 0;
}
