#include "bouncer.h"

#include <usual/slab.h>

const char *ps_version(void)
{
	return PREPARED_STATEMENT_VERSION;
}

void ps_client_socket_state_init(struct PreparedStatementSocketState* state)
{
	state->client_ps_entries = NULL;

	state->server_next_dst_ps_id = 0;
	state->server_ps_entries = NULL;
}

void ps_server_socket_state_init(struct PreparedStatementSocketState* state)
{
	state->server_next_dst_ps_id = 1;
	state->server_ps_entries = NULL;
	list_init(&state->server_outstanding_parse_packets);

	state->client_ps_entries = NULL;
}

static PgParsedPreparedStatement *create_prepared_statement(PgParsePacket *parse_packet)
{
	PgParsedPreparedStatement *s;

	s = slab_alloc(client_ps_cache);
	
	s->query_hash[0] = parse_packet->query_hash[0];
	s->query_hash[1] = parse_packet->query_hash[1];

	/* copy Parse packet */
	memcpy(s->pkt.name, parse_packet->name, sizeof(parse_packet->name));
	s->pkt.query_hash[0] = parse_packet->query_hash[0];
	s->pkt.query_hash[1] = parse_packet->query_hash[1];
	s->pkt.query_len = parse_packet->query_len;
	s->pkt.query = parse_packet->query;
	s->pkt.num_parameters = parse_packet->num_parameters;
	s->pkt.param_data_types = parse_packet->param_data_types;

	s->name = s->pkt.name;

	return s;
}

static void client_prepared_statement_free(PgSocket *client, PgParsedPreparedStatement *ps)
{
	parse_packet_free(client, &ps->pkt);
	slab_free(client_ps_cache, ps);
}

static PgServerPreparedStatement *create_server_prepared_statement(PgSocket *client, PgParsedPreparedStatement *ps)
{
	PgServerPreparedStatement *s;

	s = slab_alloc(server_ps_cache);
	*(uint64_t *)&s->query_hash[0] = ps->query_hash[0];
	*(uint64_t *)&s->query_hash[1] = ps->query_hash[1];
	s->id = client->link->ps_state.server_next_dst_ps_id++;
	s->bind_count = 0;

	return s;
}

static void server_prepared_statement_free(PgServerPreparedStatement *ps)
{
	slab_free(server_ps_cache, ps);
}

static int bind_count_sort(struct PgServerPreparedStatement *a, struct PgServerPreparedStatement *b)
{
	return (a->bind_count - b->bind_count);
}

static bool register_prepared_statement(PgSocket *server, PgServerPreparedStatement *stmt)
{
	struct PgServerPreparedStatement *current, *tmp;
	PktBuf *buf;
	unsigned int cached_query_count = HASH_COUNT(server->ps_state.server_ps_entries);

	if (cached_query_count >= (unsigned int)cf_prepared_statement_cache_queries) {
		HASH_SORT(server->ps_state.server_ps_entries, bind_count_sort);
		HASH_ITER(hh, server->ps_state.server_ps_entries, current, tmp) {
			HASH_DEL(server->ps_state.server_ps_entries, current);
			cached_query_count--;

			buf = create_close_packet(current->id);

			if (!pktbuf_send_immediate(buf, server)) {
				pktbuf_free(buf);
				return false;
			}

			pktbuf_free(buf);

			if (cf_verbose > 1) {
				dst_ps_name(stmt->id);
				slog_noise(server, "prepared statement '%s' deleted from server cache", dst_ps_name);
			}
			server_prepared_statement_free(current);

			/* update stats */
			server->ps_state.closes_total++;

			break;
		}
	}

	if (cf_verbose > 1) {
		dst_ps_name(stmt->id);
		slog_noise(server, "prepared statement '%s' added to server cache, %d cached items", dst_ps_name, cached_query_count + 1);
	}
	HASH_ADD(hh, server->ps_state.server_ps_entries, query_hash, sizeof(stmt->query_hash), stmt);

	return true;
}

bool handle_parse_command(PgSocket *client, PktHdr *pkt, const char *ps_name)
{
	SBuf *sbuf = &client->sbuf;
	PktBuf *buf;
	PgSocket *server = client->link;
	struct PgParsePacket pp;
	PgParsedPreparedStatement *ps = NULL;
	PgServerPreparedStatement *link_ps = NULL;
	struct OutstandingParsePacket *opp = NULL;

	Assert(server);

	if (!unmarshall_parse_packet(client, pkt, &pp))
		return false;

	/* update stats */
	client->ps_state.requested_parses_total++;
	client->pool->stats.ps_client_parse_count++;

	ps = create_prepared_statement(&pp);

	sbuf_prepare_skip(sbuf, pkt->len);
	if (!sbuf_flush(sbuf))
		return false;

	HASH_FIND(hh, server->ps_state.server_ps_entries, ps->query_hash, sizeof(ps->query_hash), link_ps);
	if (link_ps) {
		/* Statement already prepared on this link, do not forward packet */
		if (cf_verbose > 0) {
			dst_ps_name(link_ps->id);
			slog_debug(client, "handle_parse_command: mapping statement '%s' to '%s' (query hash '%lld/%lld')", ps->name, dst_ps_name, ps->query_hash[0], ps->query_hash[1]);
		}

		buf = create_parse_complete_packet();

		/* update stats */
		server->ps_state.requested_parses_total++;

		if (!pktbuf_send_immediate(buf, client)) {
			pktbuf_free(buf);
			return false;
		}

		pktbuf_free(buf);

		/* Register statement on client link */
		HASH_ADD_KEYPTR(hh, client->ps_state.client_ps_entries, ps->name, strlen(ps->name), ps);
	}
	else {
		/* Statement not prepared on this link, sent modified P packet */
		link_ps = create_server_prepared_statement(client, ps);

		if (cf_verbose > 0) {
			dst_ps_name(link_ps->id);
			slog_debug(client, "handle_parse_command: creating mapping for statement '%s' to '%s' (query hash '%lld/%lld')", ps->name, dst_ps_name, ps->query_hash[0], ps->query_hash[1]);
		}

		buf = create_parse_packet(client, link_ps->id, &ps->pkt);

		/* update stats */
		client->ps_state.executed_parses_total++;
		server->ps_state.executed_parses_total++;
		client->pool->stats.ps_server_parse_count++;

		/* Track Parse command sent to server */
		opp = calloc(sizeof *opp, 1);
		opp->ignore = false;
		list_append(&server->ps_state.server_outstanding_parse_packets, &opp->node);

		if (!pktbuf_send_immediate(buf, server)) {
			pktbuf_free(buf);
			return false;
		}

		pktbuf_free(buf);

		/* Register statement on client and server link */
		HASH_ADD_KEYPTR(hh, client->ps_state.client_ps_entries, ps->name, strlen(ps->name), ps);
		if (!register_prepared_statement(server, link_ps))
			return false;
	}

	return true;
}

bool handle_bind_command(PgSocket *client, PktHdr *pkt, const char *ps_name)
{
	SBuf *sbuf = &client->sbuf;
	PgSocket *server = client->link;
	PgParsedPreparedStatement *ps = NULL;
	PgServerPreparedStatement *link_ps = NULL;
	PktBuf *buf;
	struct OutstandingParsePacket *opp = NULL;

	Assert(server);

	HASH_FIND_STR(client->ps_state.client_ps_entries, ps_name, ps);
	if (!ps) {
		disconnect_client(client, true, "prepared statement '%s' not found", ps_name);
		return false;
	}

	HASH_FIND(hh, client->link->ps_state.server_ps_entries, ps->query_hash, sizeof(ps->query_hash), link_ps);

	sbuf_prepare_skip(sbuf, pkt->len);
	if (!sbuf_flush(sbuf))
		return false;

	if (!link_ps) {
		/* Statement is not prepared on this link, sent P packet first */
		link_ps = create_server_prepared_statement(client, ps);

		if (cf_verbose > 0) {
			dst_ps_name(link_ps->id);
			slog_debug(server, "handle_bind_command: prepared statement '%s' (query hash '%lld/%lld') not available on server, preparing '%s' before bind", ps->name, ps->query_hash[0], ps->query_hash[1], dst_ps_name);
		}

		buf = create_parse_packet(client, link_ps->id, &ps->pkt);

		/* update stats */
		server->ps_state.executed_parses_total++;
		client->pool->stats.ps_server_parse_count++;

		/* Track Parse command sent to server */
		opp = calloc(sizeof *opp, 1);
		opp->ignore = true;
		list_append(&server->ps_state.server_outstanding_parse_packets, &opp->node);
		if (!pktbuf_send_immediate(buf, server)) {
			pktbuf_free(buf);
			return false;
		}

		pktbuf_free(buf);

		/* Register statement on server link */
		if (!register_prepared_statement(server, link_ps))
			return false;
	}

	if (cf_verbose > 1) {
		dst_ps_name(link_ps->id);
		slog_debug(client, "handle_bind_command: mapped statement '%s' (query hash '%lld/%lld') to '%s'", ps->name, ps->query_hash[0], ps->query_hash[1], dst_ps_name);
	}

	if (!copy_bind_packet(client, &buf, link_ps->id, pkt)) {
		pktbuf_free(buf);
		return false;
	}

	if (!pktbuf_send_immediate(buf, server)) {
		pktbuf_free(buf);
		return false;
	}

	pktbuf_free(buf);

	/* update stats */
	client->ps_state.binds_total++;
	server->ps_state.binds_total++;
	link_ps->bind_count++;  // is dit een stat? of functioneel een counter
	client->pool->stats.ps_bind_count++;

	return true;
}

bool handle_describe_command(PgSocket *client, PktHdr *pkt, const char *ps_name)
{
	SBuf *sbuf = &client->sbuf;
	PgSocket *server = client->link;
	PgParsedPreparedStatement *ps = NULL;
	PgServerPreparedStatement *link_ps = NULL;
	PktBuf *buf;

	Assert(server);

	HASH_FIND_STR(client->ps_state.client_ps_entries, ps_name, ps);
	if (!ps) {
		disconnect_client(client, true, "prepared statement '%s' not found", ps_name);
		return false;
	}

	HASH_FIND(hh, client->link->ps_state.server_ps_entries, ps->query_hash, sizeof(ps->query_hash), link_ps);

	// TODO: link_ps missing -> no parse, should not be possible

	sbuf_prepare_skip(sbuf, pkt->len);
	if (!sbuf_flush(sbuf))
		return false;

	if (cf_verbose > 1) {
		dst_ps_name(link_ps->id);
		slog_debug(client, "handle_describe_command: mapped statement '%s' (query hash '%lld/%lld') to '%s'", ps->name, ps->query_hash[0], ps->query_hash[1], dst_ps_name);
	}

	buf = create_describe_packet(link_ps->id);

	if (!pktbuf_send_immediate(buf, server)) {
		pktbuf_free(buf);
		return false;
	}

	pktbuf_free(buf);

	return true;
}

bool handle_close_statement_command(PgSocket *client, PktHdr *pkt, PgClosePacket *close_packet)
{
	SBuf *sbuf = &client->sbuf;
	PgParsedPreparedStatement *ps = NULL;
	PktBuf *buf;

	HASH_FIND_STR(client->ps_state.client_ps_entries, close_packet->name, ps);
	if (ps) {
		HASH_DELETE(hh, client->ps_state.client_ps_entries, ps);
		client_prepared_statement_free(client, ps);
		slog_noise(client, "handle_close_statement_command: removed '%s' from cached prepared statements, items remaining %u", close_packet->name, HASH_COUNT(client->ps_state.client_ps_entries));

		/* Do not forward packet to server */
		sbuf_prepare_skip(sbuf, pkt->len);
		if (!sbuf_flush(sbuf))
			return false;

		buf = create_close_complete_packet();

		if (!pktbuf_send_immediate(buf, client)) {
			pktbuf_free(buf);
			return false;
		}

		pktbuf_free(buf);
	}

	/* update stats */
	client->ps_state.closes_total++;

	return true;
}

void ps_client_free(PgSocket *client)
{
	struct PgParsedPreparedStatement *current, *tmp;

	int item_cnt = HASH_COUNT(client->ps_state.client_ps_entries);
	HASH_ITER(hh, client->ps_state.client_ps_entries, current, tmp) {
		HASH_DEL(client->ps_state.client_ps_entries, current);
		client_prepared_statement_free(client, current);
	}
	slog_noise(client, "ps_client_free: freed %d prepared statements", item_cnt);
}

void ps_server_free(PgSocket *server)
{
	struct PgServerPreparedStatement *current, *tmp_s;
	struct List *el, *tmp_l;
	struct OutstandingParsePacket *opp;

	int item_cnt = HASH_COUNT(server->ps_state.server_ps_entries);
	HASH_ITER(hh, server->ps_state.server_ps_entries, current, tmp_s) {
		HASH_DEL(server->ps_state.server_ps_entries, current);
		server_prepared_statement_free(current);
	}
	slog_noise(server, "ps_server_free: freed %d prepared statements", item_cnt);

	int opp_cnt = 0;
	list_for_each_safe(el, &server->ps_state.server_outstanding_parse_packets, tmp_l) {
		opp = container_of(el, struct OutstandingParsePacket, node);
		list_del(&opp->node);
		free(opp);
		opp_cnt++;
	}
	slog_noise(server, "ps_server_free: freed %d outstanding parse packets", opp_cnt);
}

void construct_server_ps(void *obj)
{
	PgServerPreparedStatement *ps = obj;
	memset(ps, 0, sizeof(PgServerPreparedStatement));
}

void construct_client_ps(void *obj)
{
	PgParsedPreparedStatement *ps = obj;
	memset(ps, 0, sizeof(PgParsedPreparedStatement));
}
