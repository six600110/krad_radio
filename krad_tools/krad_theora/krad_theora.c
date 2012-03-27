#include "krad_theora.h"


krad_theora_encoder_t *krad_theora_encoder_create(int width, int height, int quality) {

	krad_theora_encoder_t *krad_theora;
	
	krad_theora = calloc (1, sizeof(krad_theora_encoder_t));
	
	krad_theora->width = width;
	krad_theora->height = height;
	krad_theora->quality = quality;
	
	th_info_init (&krad_theora->info);
	th_comment_init (&krad_theora->comment);

	//FIXME add support for non dividable by 16 things
	krad_theora->info.frame_width = krad_theora->width;
	krad_theora->info.frame_height = krad_theora->height;
	krad_theora->info.pic_width = krad_theora->width;
	krad_theora->info.pic_height = krad_theora->height;
	krad_theora->info.pic_x = 0;
	krad_theora->info.pic_y = 0;
	krad_theora->info.target_bitrate = 0;
	krad_theora->info.quality = krad_theora->quality;

	krad_theora->encoder = th_encode_alloc (&krad_theora->info);

	while (th_encode_flushheader ( krad_theora->encoder, &krad_theora->comment, &krad_theora->packet) > 0) {
	
		krad_theora->header[krad_theora->header_count] = malloc(krad_theora->packet.bytes);
		memcpy (krad_theora->header[krad_theora->header_count], krad_theora->packet.packet, krad_theora->packet.bytes);
		krad_theora->header_len[krad_theora->header_count] = krad_theora->packet.bytes;
		krad_theora->header_count++;
		
		
		//printf("krad_theora_encoder_create th_encode_flushheader got header packet %ld which is %ld bytes\n", 
		//		krad_theora->packet.packetno, krad_theora->packet.bytes);
	}
	
	//printf("krad_theora_encoder_create Got %d header packets\n", krad_theora->header_count);

	return krad_theora;

}

void krad_theora_encoder_destroy(krad_theora_encoder_t *krad_theora) {

	while (krad_theora->header_count--) {
		//printf("krad_theora_encoder_destroy freeing header %d\n", krad_theora->header_count);
		free (krad_theora->header[krad_theora->header_count]);
	}

	th_info_clear (&krad_theora->info);
	th_comment_clear (&krad_theora->comment);
	th_encode_free (krad_theora->encoder);
	free(krad_theora);

}

int krad_theora_encoder_write(krad_theora_encoder_t *krad_theora, unsigned char **packet, int *keyframe) {
	
	int ret;
	
	ret = th_encode_ycbcr_in (krad_theora->encoder, krad_theora->ycbcr);
	if (ret != 0) {
		printf("krad_theora_encoder_write th_encode_ycbcr_in failed! %d\n", ret);
		exit(1);
	}
	
	// Note: Currently the encoder operates in a one-frame-in, one-packet-out manner. However, this may be changed in the future.
	
	ret = th_encode_packetout (krad_theora->encoder, krad_theora->finish, &krad_theora->packet);
	if (ret < 1) {
		printf("krad_theora_encoder_write th_encode_packetout failed! %d\n", ret);
		exit(1);
	}
	
	*packet = krad_theora->packet.packet;
	
	*keyframe = th_packet_iskeyframe (&krad_theora->packet);
	if (*keyframe == -1) {
		printf("krad_theora_encoder_write th_packet_iskeyframe failed! %d\n", *keyframe);
		exit(1);
	}
	
	
	// Double check
	ogg_packet test_packet;
	ret = th_encode_packetout (krad_theora->encoder, krad_theora->finish, &test_packet);
	if (ret < 0) {
		printf("krad_theora_encoder_write th_encode_packetout offerd up an extra packet! %d\n", ret);
		exit(1);
	}
	
	return krad_theora->packet.bytes;
}



/* decoder */

void krad_theora_decoder_decode(krad_theora_decoder_t *krad_theora, void *buffer, int len) {


	//printf("theora decode with %d\n", len);
	
	krad_theora->packet.packet = buffer;
	krad_theora->packet.bytes = len;
	krad_theora->packet.packetno++;
	
	th_decode_packetin(krad_theora->decoder, &krad_theora->packet, &krad_theora->granulepos);
	
	th_decode_ycbcr_out(krad_theora->decoder, krad_theora->ycbcr);

}

void krad_theora_decoder_timecode(krad_theora_decoder_t *krad_theora, uint64_t *timecode) {

	float frame_rate;
	ogg_int64_t iframe;
	ogg_int64_t pframe;
	
	frame_rate = krad_theora->info.fps_numerator / krad_theora->info.fps_denominator;
	
	iframe = krad_theora->granulepos >> krad_theora->info.keyframe_granule_shift;
	pframe = krad_theora->granulepos - (iframe << krad_theora->info.keyframe_granule_shift);
	/* kludged, we use the default shift of 6 and assume a 3.2.1+ bitstream */
	*timecode = ((iframe + pframe - 1) / frame_rate) * 1000.0;

}


void krad_theora_decoder_destroy(krad_theora_decoder_t *krad_theora) {

    th_decode_free(krad_theora->decoder);
    th_comment_clear(&krad_theora->comment);
    th_info_clear(&krad_theora->info);
	free(krad_theora);

}

krad_theora_decoder_t *krad_theora_decoder_create(unsigned char *header1, int header1len, unsigned char *header2, int header2len, unsigned char *header3, int header3len) {

	krad_theora_decoder_t *krad_theora;
	
	krad_theora = calloc(1, sizeof(krad_theora_decoder_t));

	krad_theora->granulepos = -1;

	th_comment_init(&krad_theora->comment);
	th_info_init(&krad_theora->info);

	krad_theora->packet.packet = header1;
	krad_theora->packet.bytes = header1len;
	krad_theora->packet.b_o_s = 1;
	krad_theora->packet.packetno = 1;
	th_decode_headerin(&krad_theora->info, &krad_theora->comment, &krad_theora->setup_info, &krad_theora->packet);
	//printf("x is %d len is %d\n", x, header1len);

	krad_theora->packet.packet = header2;
	krad_theora->packet.bytes = header2len;
	krad_theora->packet.b_o_s = 0;
	krad_theora->packet.packetno = 2;
	th_decode_headerin(&krad_theora->info, &krad_theora->comment, &krad_theora->setup_info, &krad_theora->packet);
	//printf("x is %d len is %d\n", x, header2len);

	krad_theora->packet.packet = header3;
	krad_theora->packet.bytes = header3len;
	krad_theora->packet.packetno = 3;
	th_decode_headerin(&krad_theora->info, &krad_theora->comment, &krad_theora->setup_info, &krad_theora->packet);

	printf("Theora %dx%d %.02f fps video\n Encoded frame content is %dx%d with %dx%d offset\n",
		   krad_theora->info.frame_width, krad_theora->info.frame_height, 
		   (double)krad_theora->info.fps_numerator/krad_theora->info.fps_denominator,
		   krad_theora->info.pic_width, krad_theora->info.pic_height, krad_theora->info.pic_x, krad_theora->info.pic_y);


	krad_theora->offset_y = krad_theora->info.pic_y;
	krad_theora->offset_x = krad_theora->info.pic_x;

	krad_theora->width = krad_theora->info.pic_width;
	krad_theora->height = krad_theora->info.pic_height;

	krad_theora->decoder = th_decode_alloc(&krad_theora->info, krad_theora->setup_info);

	th_setup_free(krad_theora->setup_info);

	return krad_theora;

}