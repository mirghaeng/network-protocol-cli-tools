#define MAX_CONTENT_SIZE 100
#define MAX_FILENAME_LENGTH 100

typedef enum {
	no_command,
	request_filesize,
	respond_filesize,
	request_chunkcontent,
	respond_chunkcontent
} commandinfo;

typedef struct {
	commandinfo command;
	char filename[MAX_FILENAME_LENGTH];
	int byte_size;
	int start_byte;
	char *content;
} packetinfo;

packetinfo *empty_pkt()
{
	packetinfo *empty_pkt = calloc(sizeof(packetinfo) + MAX_CONTENT_SIZE, 1);
	empty_pkt->command = no_command;
	empty_pkt->byte_size = 0;
	empty_pkt->start_byte = 0;
	return empty_pkt;
}

packetinfo *revised_pkt(commandinfo command, int byte_size, int start_byte, char* filename, char* content)
{
	packetinfo* revised_pkt = calloc(sizeof(packetinfo) + byte_size, 1);
	revised_pkt->command = command;
	revised_pkt->byte_size = byte_size;
	revised_pkt->start_byte = start_byte;
	strncpy(revised_pkt->filename, filename, strlen(filename)+1);
	revised_pkt->filename[strlen(filename)+1] = '\0';
	if(content!=NULL) memcpy(&(revised_pkt->content), content, byte_size);
	return revised_pkt;
}
