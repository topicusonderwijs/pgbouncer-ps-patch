import psycopg
import pytest

from .utils import TEST_DIR, Bouncer

# override regular bouncer fixture with one that uses the special prepared statement config
# (currently we do not support enabling prepared statement support on the fly)
@pytest.mark.asyncio
@pytest.fixture
async def bouncer(pg, tmp_path):
    bouncer = Bouncer(
        pg, tmp_path / "bouncer", base_ini_path=TEST_DIR / "prepared_statement" / "test.ini"
    )

    await bouncer.start()

    yield bouncer

    await bouncer.cleanup()


def test_prepared_statement(bouncer):
    prepared_query = "SELECT 1"
    with bouncer.cur() as cur1:
        with bouncer.cur() as cur2:
            cur1.execute(prepared_query, prepare=True)
            cur1.execute(prepared_query)
            with cur2.connection.transaction():
                cur2.execute("SELECT 2")
                cur1.execute(prepared_query)
