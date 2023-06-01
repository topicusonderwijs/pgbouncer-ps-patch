import psycopg
import pytest

def test_prepared_statement(bouncer):
    bouncer.admin(f"set pool_mode=transaction")
    bouncer.admin(f"set prepared_statement_cache_queries=11")

    prepared_query = "SELECT 1"
    with bouncer.cur() as cur1:
        with bouncer.cur() as cur2:
            cur1.execute(prepared_query, prepare=True)
            cur1.execute(prepared_query)
            with cur2.connection.transaction():
                cur2.execute("SELECT 2")
                cur1.execute(prepared_query)
