#define MAX_STATEMENT_NAME	24

#define QUERY_EXTENT_SIZE 256

#define QUERY_PARAM_DATA_TYPE_EXTENT_SIZE 16

typedef struct PgQueryExtent
{
	char data[QUERY_EXTENT_SIZE];
	struct PgQueryExtent *next;
} PgQueryExtent;

typedef struct PgParamDataTypeList
{
	uint8_t values[QUERY_PARAM_DATA_TYPE_EXTENT_SIZE * sizeof(u_int32_t)];
	struct PgParamDataTypeList *next;
} PgParamDataTypeList;

typedef struct PgParsePacket
{
	char name[MAX_STATEMENT_NAME];
	uint64_t query_hash[2];
	uint64_t query_len;
	struct PgQueryExtent *query;
	uint16_t num_parameters;
	struct PgParamDataTypeList *param_data_types;
} PgParsePacket;

typedef struct PgClosePacket
{
	char type;
	char name[MAX_STATEMENT_NAME];
} PgClosePacket;

#define dst_ps_name(dst_ps_id)                                             \
	char dst_ps_name[MAX_STATEMENT_NAME];                                  \
	snprintf(dst_ps_name, sizeof(dst_ps_name), "B_%lld", dst_ps_id);

void ps_init(void);

void construct_query_extent(void *obj);
void construct_param_list(void *obj);

bool inspect_parse_packet(PgSocket *client, PktHdr *pkt, char *dst_p);
bool inspect_bind_packet(PgSocket *client, PktHdr *pkt, char *dst_p);
bool inspect_describe_packet(PgSocket *client, PktHdr *pkt, char *dst_p);

bool unmarshall_parse_packet(PgSocket *client, PktHdr *pkt, PgParsePacket *parse_packet_p);
bool unmarshall_close_packet(PgSocket *client, PktHdr *pkt, PgClosePacket *close_packet_p);

bool is_close_statement_packet(PgClosePacket *close_packet);

PktBuf *create_parse_packet(uint64_t dst_ps_id, PgParsePacket *parse_packet);
PktBuf *create_parse_complete_packet(void);
PktBuf *create_describe_packet(uint64_t dst_ps_id);
PktBuf *create_close_packet(uint64_t dst_ps_id);
PktBuf *create_close_complete_packet(void);

bool copy_bind_packet(PgSocket *client, PktBuf **buf_p, uint64_t dst_ps_id, PktHdr *pkt);

void parse_packet_free(PgParsePacket *pkt);
