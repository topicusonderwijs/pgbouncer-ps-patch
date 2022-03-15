#include "common/uthash.h"

typedef struct PgParsedPreparedStatement
{
	const char *name;
  uint64_t query_hash[2];
  PgParsePacket *pkt;
  UT_hash_handle hh;
} PgParsedPreparedStatement;

typedef struct PgServerPreparedStatement
{
  const uint64_t query_hash[2];
  char *name;
  uint64_t bind_count;
  UT_hash_handle hh;
} PgServerPreparedStatement;

bool handle_parse_command(PgSocket *client, PktHdr *pkt, const char *ps_name);
bool handle_bind_command(PgSocket *client, PktHdr *pkt, const char *ps_name);
bool handle_close_statement_command(PgSocket *client, PktHdr *pkt, PgClosePacket *close_packet);

void ps_client_free(PgSocket *client);
void ps_server_free(PgSocket *server);