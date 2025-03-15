#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <stdio.h>

int main() {
    // Initialize FFmpeg
    av_register_all();
    avformat_network_init();

    // Input RTSP URL (replace with your camera's RTSP URL)
    const char *rtsp_url = "rtsp://<camera_ip>:<port>/<stream_path>";
    const char *output_file = "output.mp4";

    // Open the RTSP stream
    AVFormatContext *input_ctx = NULL;
    if (avformat_open_input(&input_ctx, rtsp_url, NULL, NULL) < 0) {
        fprintf(stderr, "Error: Could not open RTSP stream.\n");
        return -1;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(input_ctx, NULL) < 0) {
        fprintf(stderr, "Error: Could not find stream information.\n");
        return -1;
    }

    // Open the output file
    AVFormatContext *output_ctx = NULL;
    avformat_alloc_output_context2(&output_ctx, NULL, NULL, output_file);
    if (!output_ctx) {
        fprintf(stderr, "Error: Could not create output context.\n");
        return -1;
    }

    // Copy streams from input to output
    for (int i = 0; i < input_ctx->nb_streams; i++) {
        AVStream *in_stream = input_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(output_ctx, NULL);
        if (!out_stream) {
            fprintf(stderr, "Error: Could not allocate output stream.\n");
            return -1;
        }
        avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        out_stream->codecpar->codec_tag = 0;
    }

    // Open the output file for writing
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_ctx->pb, output_file, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Error: Could not open output file.\n");
            return -1;
        }
    }

    // Write the header to the output file
    if (avformat_write_header(output_ctx, NULL) < 0) {
        fprintf(stderr, "Error: Could not write header to output file.\n");
        return -1;
    }

    // Read frames from the RTSP stream and write them to the output file
    AVPacket pkt;
    while (av_read_frame(input_ctx, &pkt) >= 0) {
        // Write the frame to the output file
        av_interleaved_write_frame(output_ctx, &pkt);
        av_packet_unref(&pkt);
    }

    // Write the trailer to the output file
    av_write_trailer(output_ctx);

    // Clean up
    avformat_close_input(&input_ctx);
    if (output_ctx && !(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&output_ctx->pb);
    }
    avformat_free_context(output_ctx);

    printf("Stream saved to %s\n", output_file);
    return 0;
}