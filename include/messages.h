typedef struct PgParsePacket
{
  char *name;
  char *query;
  uint16_t num_parameters;
  uint8_t *parameter_types_bytes;
} PgParsePacket;

typedef struct PgClosePacket
{
	char type;
  char* name;
} PgClosePacket;

bool inspect_parse_packet(PgSocket *client, PktHdr *pkt, const char **dst_p);
bool inspect_bind_packet(PgSocket *client, PktHdr *pkt, const char **dst_p);

bool unmarshall_parse_packet(PgSocket *client, PktHdr *pkt, PgParsePacket **parse_packet_p);
bool unmarshall_close_packet(PgSocket *client, PktHdr *pkt, PgClosePacket **close_packet_p);

PktBuf *create_parse_packet(char *statement, PgParsePacket *parse_packet);
PktBuf *create_parse_complete_packet(void);
PktBuf *create_close_packet(char *statement);

bool copy_bind_packet(PgSocket *client, PktBuf **buf_p, char *statement, PktHdr *pkt);

void parse_packet_free(PgParsePacket *pkt);
