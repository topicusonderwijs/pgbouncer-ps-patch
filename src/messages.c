#include "bouncer.h"

static bool handle_incomplete_packet(PgSocket *client, PktHdr *pkt)
{
  if (incomplete_pkt(pkt))
  {
    /* 256 is magic number: packet does fit in buffer, but buffer not filled by sync process somehow */
    if ((int)pkt->len > (int)cf_sbuf_len - 256) {
      slog_error(client, "Packet length (%d) bigger than buffer size (%d - 256)", pkt->len, cf_sbuf_len);
      disconnect_client(client, false, "Query too large for buffer - disconnecting");
    }
    return false;
  }
  return true;
}

bool inspect_parse_packet(PgSocket *client, PktHdr *pkt, const char **dst_p)
{
  const uint8_t *ptr;
  const char *statement;

  if (!handle_incomplete_packet(client, pkt))
    return false;

  mbuf_rewind_reader(&pkt->data);

  /* Start at 5, because we skip the 'P' and the 4 bytes which are the length of the message */
  if (!mbuf_get_bytes(&pkt->data, 5, &ptr))
    goto failed;

  if (!mbuf_get_string(&pkt->data, &statement))
    goto failed;

  if (strlen(statement) > 0) {
    *dst_p = strdup(statement);
    slog_noise(client, "inspect_parse_packet: type=%c, len=%d, statement=%s", pkt->type, pkt->len, statement);
  } else {
    *dst_p = NULL;
    slog_noise(client, "inspect_parse_packet: type=%c, len=%d, statement=<empty>", pkt->type, pkt->len);
  }

  mbuf_rewind_reader(&pkt->data);
  
  return true;

	failed:
    disconnect_client(client, true, "broken Parse packet");
	  return false;
}

bool inspect_bind_packet(PgSocket *client, PktHdr *pkt, const char **dst_p)
{
  const uint8_t *ptr;
  const char *portal;
  const char *statement;

  if (!handle_incomplete_packet(client, pkt))
    return false;

  mbuf_rewind_reader(&pkt->data);

  /* Start at 5, because we skip the 'B' and the 4 bytes which are the length of the message */
  if (!mbuf_get_bytes(&pkt->data, 5, &ptr))
    goto failed;

  if (!mbuf_get_string(&pkt->data, &portal))
    goto failed;

  if (!mbuf_get_string(&pkt->data, &statement))
    goto failed;

  if (strlen(statement) > 0) {
    slog_noise(client, "inspect_bind_packet: type=%c, len=%d, statement=%s", pkt->type, pkt->len, statement);
    *dst_p = strdup(statement);
  } else {
    *dst_p = NULL;
    slog_noise(client, "inspect_bind_packet: type=%c, len=%d, statement=<empty>", pkt->type, pkt->len);
  }

  mbuf_rewind_reader(&pkt->data);
  
  return true;

	failed:
    disconnect_client(client, true, "broken Bind packet");
	  return false;
}

bool inspect_describe_packet(PgSocket *client, PktHdr *pkt, const char **dst_p)
{
  const uint8_t *ptr;
  char describe;
  const char *statement;

if (!handle_incomplete_packet(client, pkt))
    return false;

  mbuf_rewind_reader(&pkt->data);

  /* Start at 5, because we skip the 'D' and the 4 bytes which are the length of the message */
  if (!mbuf_get_bytes(&pkt->data, 5, &ptr))
    goto failed;

  if (!mbuf_get_char(&pkt->data, &describe))
    goto failed;

  if (describe == 'S') {
    if (!mbuf_get_string(&pkt->data, &statement))
      goto failed;

    if (strlen(statement) > 0) {
      slog_noise(client, "inspect_describe_packet: type=%c, len=%d, P/S=%c, statement=%s", pkt->type, pkt->len, describe, statement);
      *dst_p = strdup(statement);
    } else {
      *dst_p = NULL;
      slog_noise(client, "inspect_descibe_packet: type=%c, len=%d, P/S=%c, statement=<empty>", pkt->type, pkt->len, describe);
    }
  } else {
    *dst_p = NULL;
    slog_noise(client, "inspect_descibe_packet: type=%c, len=%d, P/S=%c", pkt->type, pkt->len, describe);
  }

  mbuf_rewind_reader(&pkt->data);

  return true;

	failed:
    disconnect_client(client, true, "broken Describe packet");
	  return false;
}

bool unmarshall_parse_packet(PgSocket *client, PktHdr *pkt, PgParsePacket **parse_packet_p)
{
  const uint8_t *ptr;
  const char* statement;
  const char* query;
  uint16_t num_parameters;
  const uint8_t *parameter_types_bytes;

  if (!handle_incomplete_packet(client, pkt))
    return false;

  mbuf_rewind_reader(&pkt->data);

  /* Skip first 5 bytes, because we skip the 'P' and the 4 bytes which are the length of the message */
  if (!mbuf_get_bytes(&pkt->data, 5, &ptr))
    goto failed;

  if (!mbuf_get_string(&pkt->data, &statement))
    goto failed;

  if (!mbuf_get_string(&pkt->data, &query))
    goto failed;
    
  /* number of parameter data types */
  if (!mbuf_get_uint16be(&pkt->data, &num_parameters))
    goto failed;

  if (!mbuf_get_bytes(&pkt->data, num_parameters * 4, &parameter_types_bytes))
    goto failed;
 
  *parse_packet_p = (PgParsePacket *)malloc(sizeof(PgParsePacket));
  (*parse_packet_p)->name = strdup(statement);
  (*parse_packet_p)->query = strdup(query);
  (*parse_packet_p)->num_parameters = num_parameters;
  (*parse_packet_p)->parameter_types_bytes = (uint8_t *)malloc(4 * num_parameters);
  memcpy((*parse_packet_p)->parameter_types_bytes, parameter_types_bytes, num_parameters * 4);

  return true;

	failed:
    disconnect_client(client, true, "broken Parse packet");
	  return false;
}

bool unmarshall_close_packet(PgSocket *client, PktHdr *pkt, PgClosePacket **close_packet_p)
{
  const uint8_t *ptr;
  char type;
  const char *name;

  if (!handle_incomplete_packet(client, pkt))
    return false;

  mbuf_rewind_reader(&pkt->data);
  
  if (!mbuf_get_bytes(&pkt->data, 5, &ptr))
    goto failed;

  if (!mbuf_get_char(&pkt->data, &type))
    return true;

  if (!mbuf_get_string(&pkt->data, &name))
    name = "";

  *close_packet_p = (PgClosePacket *)malloc(sizeof(PgClosePacket));
  (*close_packet_p)->type = type;
  (*close_packet_p)->name = strdup(name);

  slog_noise(client, "unmarshall_close_packet: type=%c, len=%d, S/P=%c, name=%s", pkt->type, pkt->len, type, name);

  return true;

	failed:
    disconnect_client(client, true, "broken Close packet");
	  return false;
}

bool is_close_statement_packet(PgClosePacket *close_packet)
{
  return close_packet->type == 'S' && strlen(close_packet->name) > 0;
}

PktBuf *create_parse_packet(char *statement, PgParsePacket *pkt)
{
  PktBuf *buf;
  buf = pktbuf_dynamic(5);
  pktbuf_start_packet(buf, 'P');
  pktbuf_put_string(buf, statement);
  pktbuf_put_string(buf, pkt->query);
  pktbuf_put_uint16(buf, pkt->num_parameters);
  pktbuf_put_bytes(buf, pkt->parameter_types_bytes, pkt->num_parameters * 4);
  pktbuf_finish_packet(buf);
  return buf;
}

PktBuf *create_parse_complete_packet(void)
{
  PktBuf *buf;
  buf = pktbuf_dynamic(5);
  pktbuf_start_packet(buf, '1');
  pktbuf_finish_packet(buf);
  return buf;
}

PktBuf *create_describe_packet(char *statement)
{
  PktBuf *buf;
  buf = pktbuf_dynamic(6 + strlen(statement));
  pktbuf_start_packet(buf, 'D');
  pktbuf_put_char(buf, 'S');
  pktbuf_put_string(buf, statement);
  pktbuf_finish_packet(buf);
  return buf;
}

PktBuf *create_close_packet(char *statement)
{
  PktBuf *buf;
  buf = pktbuf_dynamic(6 + strlen(statement));
  pktbuf_start_packet(buf, 'C');
  pktbuf_put_char(buf, 'S');
  pktbuf_put_string(buf, statement);
  pktbuf_finish_packet(buf);
  return buf;
}

PktBuf *create_close_complete_packet(void)
{
  PktBuf *buf;
  buf = pktbuf_dynamic(5);
  pktbuf_start_packet(buf, '3');
  pktbuf_finish_packet(buf);
  return buf;
}

bool copy_bind_packet(PgSocket *client, PktBuf **buf_p, char *remapped_statement, PktHdr *pkt)
{
  PktBuf *buf;
  const uint8_t *ptr;
  const char *portal;
  const char *statement;
  uint16_t i, len, val;
  uint32_t val32;

  if (!handle_incomplete_packet(client, pkt))
    return false;

  mbuf_rewind_reader(&pkt->data);

  buf = pktbuf_dynamic(pkt->len);
  pktbuf_start_packet(buf, 'B');

  /* Skip first 5 bytes, because we skip the 'B' and the 4 bytes which are the length of the message */
  if (!mbuf_get_bytes(&pkt->data, 5, &ptr))
    goto failed;

  if (!mbuf_get_string(&pkt->data, &portal))
    goto failed;
  pktbuf_put_string(buf, portal);

  if (!mbuf_get_string(&pkt->data, &statement))
    goto failed;
  pktbuf_put_string(buf, remapped_statement);

  /* Number of parameter format codes */
  if (!mbuf_get_uint16be(&pkt->data, &len))
    goto failed;
  pktbuf_put_uint16(buf, len);

  /* Parameter format codes */
  for (i=0; i < len; i++) {
    if (!mbuf_get_uint16be(&pkt->data, &val))
      goto failed;
    pktbuf_put_uint16(buf, val);
  }

  /* Number of parameter values */
  if (!mbuf_get_uint16be(&pkt->data, &len))
    goto failed;
  pktbuf_put_uint16(buf, len);

  /* Parameter values */
  for (i=0; i < len; i++) {
    /* Length of the parameter value (Int32) */
    if (!mbuf_get_uint32be(&pkt->data, &val32))
      goto failed;
    pktbuf_put_uint32(buf, val32);

    /* -1 indicates a NULL parameter value, no value bytes follow in the NULL case */
    if ((int32_t)val32 == -1)
      continue;

    /* Value of the parameter */
    if (!mbuf_get_bytes(&pkt->data, val32, &ptr))
      goto failed;
    pktbuf_put_bytes(buf, ptr, val32);
  }

  /* Number of result columns format codes */
  if (!mbuf_get_uint16be(&pkt->data, &len))
    goto failed;
  pktbuf_put_uint16(buf, len);

  /* result columns format codes */
  for (i=0; i < len; i++) {
    if (!mbuf_get_uint16be(&pkt->data, &val))
      goto failed;
    pktbuf_put_uint16(buf, val);
  }

 	pktbuf_finish_packet(buf);

  *buf_p = buf;
  return true;

	failed:
    disconnect_client(client, true, "broken Bind packet");
	  return false;
}

void parse_packet_free(PgParsePacket *pkt)
{
  free(pkt->name);
  free(pkt->query);
  free(pkt->parameter_types_bytes);
  free(pkt);
}