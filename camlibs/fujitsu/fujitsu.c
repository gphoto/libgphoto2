char *fujitsu_build_packet (char type, char subtype, int data_length) {

	char *packet;

	packet = (char*)malloc(sizeof(char)*(data_length+4));

	packet[0] = type;
	packet[1] = subtype;
	packet[2] = data_length;

	return (packet);
}


int fujitsu_process_packet (char *packet) {

	short int x, checksum=0;
	int length = 4 + packet[2];

	packet[2] = htons(packet[2]);
	for (x=3; x<length-1; x++) {
		packet[x] = htons(packet[x]);
		checksum+=packet[x];
	}

	packet[length-1] = htons(checksum);

	return (length);
}
