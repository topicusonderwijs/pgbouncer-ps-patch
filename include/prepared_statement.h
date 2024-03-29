/*
 * PgBouncer - Lightweight connection pooler for PostgreSQL.
 *
 * Copyright (c) 2022-2023 Marko Kreen
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Named prepared statement support.
 */

#include "common/uthash.h"

typedef struct PgParsedPreparedStatement
{
	const char *name;
	uint64_t query_hash[2];
	struct PgParsePacket pkt;
	UT_hash_handle hh;
} PgParsedPreparedStatement;

typedef struct PgServerPreparedStatement
{
	const uint64_t query_hash[2];
	uint64_t id;
	uint64_t bind_count;
	UT_hash_handle hh;
} PgServerPreparedStatement;

typedef struct PreparedStatementSocketState
{
	PgParsedPreparedStatement *client_ps_entries;

	uint64_t server_next_dst_ps_id;
	PgServerPreparedStatement *server_ps_entries;
	struct List server_outstanding_parse_packets;

	uint64_t requested_parses_total;
	uint64_t executed_parses_total;
	uint64_t binds_total;
	uint64_t closes_total;
} PreparedStatementSocketState;

#define is_prepared_statements_enabled(pool)                                                \
	(pool_pool_mode(pool) == POOL_TX && pool_prepared_statement_cache_queries(pool) > 0)

const char *ps_version(void);

void ps_client_socket_state_init(struct PreparedStatementSocketState* state);
void ps_server_socket_state_init(struct PreparedStatementSocketState* state);

void construct_server_ps(void *obj);
void construct_client_ps(void *obj);

bool handle_parse_command(PgSocket *client, PktHdr *pkt, const char *ps_name);
bool handle_bind_command(PgSocket *client, PktHdr *pkt, const char *ps_name);
bool handle_describe_command(PgSocket *client, PktHdr *pkt, const char *ps_name);
bool handle_close_statement_command(PgSocket *client, PktHdr *pkt, PgClosePacket *close_packet);

void ps_client_free(PgSocket *client);
void ps_server_free(PgSocket *server);
