#include "bouncer.h"

#include <usual/crypto/csrandom.h>
#include <usual/hashing/spooky.h>
#include <usual/slab.h>

static uint32_t rand_seed = 0;

void ps_init(void) {
	rand_seed = csrandom();
}

static bool handle_incomplete_packet(PgSocket *client, PktHdr *pkt)
{
	if (incomplete_pkt(pkt)) {
		/* 256 is magic number: packet does fit in buffer, but buffer not filled by sync process somehow */
		if ((int)pkt->len > (int)cf_sbuf_len - 256) {
			slog_error(client, "Packet length (%d) bigger than buffer size (%d - 256)", pkt->len, cf_sbuf_len);
			disconnect_client(client, false, "Query too large for buffer - disconnecting");
		}
		return false;
	}
	return true;
}

bool inspect_parse_packet(PgSocket *client, PktHdr *pkt, char *dst_p)
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

	if (strlcpy(dst_p, statement, MAX_STATEMENT_NAME) >= MAX_STATEMENT_NAME) {
		slog_error(client, "statement name '%s' too long (max %d)", statement, MAX_STATEMENT_NAME);
		goto failed;
	}

	slog_noise(client, "inspect_parse_packet: type=%c, pkt_len=%d, statement=%s", pkt->type, pkt->len, strlen(statement) > 0 ? statement : "<empty>");

	mbuf_rewind_reader(&pkt->data);

	return true;

failed:
	disconnect_client(client, true, "broken Parse packet");
	return false;
}

bool inspect_bind_packet(PgSocket *client, PktHdr *pkt, char *dst_p)
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

	if (strlcpy(dst_p, statement, MAX_STATEMENT_NAME) >= MAX_STATEMENT_NAME) {
		slog_error(client, "statement name '%s' too long (max %d)", statement, MAX_STATEMENT_NAME);
		goto failed;
	}

	slog_noise(client, "inspect_bind_packet: type=%c, pkt_len=%d, statement=%s", pkt->type, pkt->len, dst_p);

	mbuf_rewind_reader(&pkt->data);

	return true;

failed:
	disconnect_client(client, true, "broken Bind packet");
	return false;
}

bool inspect_describe_packet(PgSocket *client, PktHdr *pkt, char *dst_p)
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

		if (strlcpy(dst_p, statement, MAX_STATEMENT_NAME) >= MAX_STATEMENT_NAME) {
			slog_error(client, "statement name '%s' too long (max %d)", statement, MAX_STATEMENT_NAME);
			goto failed;
		}
	}

	slog_noise(client, "inspect_describe_packet: type=%c, pkt_len=%d, P/S=%c, statement=%s", pkt->type, pkt->len, describe, strlen(dst_p) > 0 ? dst_p : "<empty>");

	mbuf_rewind_reader(&pkt->data);

	return true;

failed:
	disconnect_client(client, true, "broken Describe packet");
	return false;
}

static void copy_query(PgQueryExtent *extent, const char *query, int offset)
{
	unsigned long len = strnlen(query + offset, sizeof(extent->data) + 1);
	strncpy(extent->data, query + offset, sizeof(extent->data));

	if (len > sizeof(extent->data)) {
		extent->next = slab_alloc(client_ps_query_cache);
		copy_query(extent->next, query, offset + sizeof(extent->data));
	}
}

static void copy_bind_param_data_types(PgParamDataTypeList *list, const uint8_t *parameter_types_bytes, uint16_t num_parameters)
{
	if (num_parameters > QUERY_PARAM_DATA_TYPE_EXTENT_SIZE) {
		memcpy(list->values, parameter_types_bytes, sizeof(list->values));
		list->next = slab_alloc(client_ps_param_cache);
		copy_bind_param_data_types(list->next, parameter_types_bytes + sizeof(list->values), num_parameters - QUERY_PARAM_DATA_TYPE_EXTENT_SIZE);
	} else {
		memcpy(list->values, parameter_types_bytes, num_parameters * sizeof(u_int32_t));
	}
}

bool unmarshall_parse_packet(PgSocket *client, PktHdr *pkt, PgParsePacket *parse_packet_p)
{
	const uint8_t *ptr;
	const char *statement;
	const char *query;
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

	if (!mbuf_get_bytes(&pkt->data, num_parameters * sizeof(u_int32_t), &parameter_types_bytes))
		goto failed;

	strlcpy(parse_packet_p->name, statement, MAX_STATEMENT_NAME);

	parse_packet_p->query_len = strlen(query);

	parse_packet_p->query_hash[0] = rand_seed;
	parse_packet_p->query_hash[1] = 0;
	spookyhash(query, parse_packet_p->query_len, &parse_packet_p->query_hash[0], &parse_packet_p->query_hash[1]);

	parse_packet_p->query = slab_alloc(client_ps_query_cache);
	copy_query(parse_packet_p->query, query, 0);

	parse_packet_p->num_parameters = num_parameters;
	if (num_parameters > 0) {
		parse_packet_p->param_data_types = slab_alloc(client_ps_param_cache);
		copy_bind_param_data_types(parse_packet_p->param_data_types, parameter_types_bytes, num_parameters);
	}

	slog_noise(client, "unmarshall_parse_packet: statement=%s, query_len=%llu, params=%d", statement, parse_packet_p->query_len, parse_packet_p->num_parameters);

	return true;

failed:
	disconnect_client(client, true, "broken Parse packet");
	return false;
}

bool unmarshall_close_packet(PgSocket *client, PktHdr *pkt, PgClosePacket *close_packet_p)
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

	close_packet_p->type = type;
	strlcpy(close_packet_p->name, name, MAX_STATEMENT_NAME);

	slog_noise(client, "unmarshall_close_packet: type=%c, pkt_len=%d, S/P=%c, statement=%s", pkt->type, pkt->len, type, name);

	return true;

failed:
	disconnect_client(client, true, "broken Close packet");
	return false;
}

bool is_close_statement_packet(PgClosePacket *close_packet)
{
	return close_packet->type == 'S' && strlen(close_packet->name) > 0;
}

PktBuf *create_parse_packet(uint64_t dst_ps_id, PgParsePacket *pkt)
{
	PgQueryExtent *query;
	PgParamDataTypeList *paramTypes;
	uint16_t offset = 0;

	dst_ps_name(dst_ps_id);

	PktBuf *buf;
	int pkt_len = 5 + strlen(dst_ps_name) + 1 + pkt->query_len + 1 + sizeof(u_int16_t) + (pkt->num_parameters * sizeof(u_int32_t));
	buf = pktbuf_dynamic(pkt_len);
	pktbuf_start_packet(buf, 'P');
	pktbuf_put_string(buf, dst_ps_name);
	query = pkt->query;
	while (query) {
		long len = query->next ? QUERY_EXTENT_SIZE : strlen(query->data) + 1;
		pktbuf_put_bytes(buf, query->data, len);
		query = query->next;
	}
	pktbuf_put_uint16(buf, pkt->num_parameters);
	if (pkt->num_parameters > 0) {
		paramTypes = pkt->param_data_types;
		while (offset <= pkt->num_parameters) {
			long len = pkt->num_parameters - offset > QUERY_PARAM_DATA_TYPE_EXTENT_SIZE ? QUERY_PARAM_DATA_TYPE_EXTENT_SIZE : pkt->num_parameters - offset; 
			pktbuf_put_bytes(buf, paramTypes->values, len * sizeof(u_int32_t));
			offset += QUERY_PARAM_DATA_TYPE_EXTENT_SIZE;
			paramTypes = paramTypes->next;
		}
	}

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

PktBuf *create_describe_packet(uint64_t dst_ps_id)
{
	dst_ps_name(dst_ps_id);

	PktBuf *buf;
	buf = pktbuf_dynamic(6 + strlen(dst_ps_name));
	pktbuf_start_packet(buf, 'D');
	pktbuf_put_char(buf, 'S');
	pktbuf_put_string(buf, dst_ps_name);
	pktbuf_finish_packet(buf);
	return buf;
}

PktBuf *create_close_packet(uint64_t dst_ps_id)
{
	dst_ps_name(dst_ps_id);

	PktBuf *buf;
	buf = pktbuf_dynamic(6 + strlen(dst_ps_name));
	pktbuf_start_packet(buf, 'C');
	pktbuf_put_char(buf, 'S');
	pktbuf_put_string(buf, dst_ps_name);
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

bool copy_bind_packet(PgSocket *client, PktBuf **buf_p, uint64_t dst_ps_id, PktHdr *pkt)
{
	PktBuf *buf;
	const uint8_t *ptr;
	const char *portal;
	const char *statement;
	uint16_t i, len_param_fmt_codes, len_param_values, len_result_column_format_codes, val;
	uint32_t val32;

	if (!handle_incomplete_packet(client, pkt))
		return false;

	mbuf_rewind_reader(&pkt->data);

	buf = pktbuf_dynamic(5 + pkt->len);
	pktbuf_start_packet(buf, 'B');

	/* Skip first 5 bytes, because we skip the 'B' and the 4 bytes which are the length of the message */
	if (!mbuf_get_bytes(&pkt->data, 5, &ptr))
		goto failed;

	if (!mbuf_get_string(&pkt->data, &portal))
		goto failed;
	pktbuf_put_string(buf, portal);

	if (!mbuf_get_string(&pkt->data, &statement))
		goto failed;
	dst_ps_name(dst_ps_id);
	pktbuf_put_string(buf, dst_ps_name);

	/* Number of parameter format codes */
	if (!mbuf_get_uint16be(&pkt->data, &len_param_fmt_codes))
		goto failed;
	pktbuf_put_uint16(buf, len_param_fmt_codes);

	/* Parameter format codes */
	for (i = 0; i < len_param_fmt_codes; i++) {
		if (!mbuf_get_uint16be(&pkt->data, &val))
			goto failed;
		pktbuf_put_uint16(buf, val);
	}

	/* Number of parameter values */
	if (!mbuf_get_uint16be(&pkt->data, &len_param_values))
		goto failed;
	pktbuf_put_uint16(buf, len_param_values);

	/* Parameter values */
	for (i = 0; i < len_param_values; i++) {
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
	if (!mbuf_get_uint16be(&pkt->data, &len_result_column_format_codes))
		goto failed;
	pktbuf_put_uint16(buf, len_result_column_format_codes);

	/* result columns format codes */
	for (i = 0; i < len_result_column_format_codes; i++) {
		if (!mbuf_get_uint16be(&pkt->data, &val))
			goto failed;
		pktbuf_put_uint16(buf, val);
	}

	pktbuf_finish_packet(buf);

	slog_noise(client, \
		"copy_bind_packet: portal=%s, src_statement=%s, dst_statement=%s, nr_param_fmt_codes=%u, nr_param_values=%u, nr_result_column_fmt_codes=%u", \
		portal, statement, dst_ps_name, len_param_fmt_codes, len_param_values, len_result_column_format_codes);

	*buf_p = buf;
	return true;

failed:
	disconnect_client(client, true, "broken Bind packet");
	return false;
}

void construct_query_extent(void *obj)
{
	PgQueryExtent *q = obj;
	memset(q, 0, sizeof(PgQueryExtent));
	q->next = NULL;
}

void construct_param_list(void *obj)
{
	PgParamDataTypeList *l = obj;
	memset(l, 0, sizeof(PgParamDataTypeList));
	l->next = NULL;
}

static void query_free(PgQueryExtent *extent)
{
	if (extent->next)
		query_free(extent->next);

	slab_free(client_ps_query_cache, extent);
}

static void param_data_types_free(PgParamDataTypeList *list)
{
	if (list->next)
		param_data_types_free(list->next);

	slab_free(client_ps_param_cache, list);
}

void parse_packet_free(PgParsePacket *pkt)
{
	query_free(pkt->query);

	if (pkt->num_parameters > 0)
		param_data_types_free(pkt->param_data_types);
}
