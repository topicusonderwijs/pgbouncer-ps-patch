[![GitHub license](https://img.shields.io/github/license/Naereen/StrapDown.js.svg)](https://github.com/topicus-education-ops/pgbouncer-ps-patch/blob/master/LICENSE.md)
[![Generic badge](https://img.shields.io/badge/release-1.14.0-blue.svg)](https://github.com/pgbouncer/pgbouncer/releases/tag/pgbouncer_1_14_0)


# PgBouncer with prepared statement support
Unfortunately [PgBouncer](https://www.pgbouncer.org/) does not support prepared statements in transaction pooling mode, as stated in the FAQ:
> To make prepared statements work in this mode would need PgBouncer to keep track of them internally, which it does not do. So the only way to keep using PgBouncer in this mode is to disable prepared statements in the client.

This patch introduces [prepared statement tracking](#how-it-works), allowing to run PgBouncer in transaction pooling mode with server side prepared statements. It 
**only** supports the extended query protocol parse, bind, execute and close [message formats](https://www.postgresql.org/docs/12/protocol-message-formats.html).
E.g. the flow used by the PostgreSQL JDBC driver to create server prepared statements.

# Getting Started

### Install
Download and install pgbouncer-ps by running the following commands (RHEL/CentOS):
```
# install required packages - see https://github.com/pgbouncer/pgbouncer#building
sudo yum install libevent-devel openssl-devel python-devel libtool git patch make -y

# download the latest tested pgbouncer distribution - 1.17
git clone https://github.com/pgbouncer/pgbouncer.git --branch "pgbouncer_1_17_0"

# download pgbouncer-ps extensions - tag v1.17.0+1
git clone git@github.com:topicusonderwijs/pgbouncer-ps-patch.git

# merge pgbouncer-ps extensions into pgbouncer code
cd pgbouncer-ps-patch
./install-pgbouncer-ps-patch.sh ../pgbouncer

# build and install
cd ../pgbouncer
git submodule init
git submodule update
./autogen.sh
./configure ...
make
sudo make install
```

### Configure
Create a configuration file, using `./etc/pgbouncer.ini` as a starting point.

By default, prepared statement support is not enabled. To enable it just add the following to your pgbouncer-ps configuration:
```
disable_prepared_statement_support = 0
```

The number of prepared statements kept active on a single backend connection is defined by the following configuration:
```
prepared_statement_cache_queries = 100
```
Note: keep in mind that this will increase the memory footprint of each client connection on your PostgreSQL server.

Tracking prepared statements does not only come with a memory cost, but also with increased cpu usage. Multiple PgBouncer instances can be run to use more than one core for processing, see `so_reuseport` socket option.

### Connect the application
Configure your client application as though you were connecting directly to a PostgreSQL database.

Example - JDBC driver
```
jdbc:postgresql://pgbouncer-ps:6432/postgres?prepareThreshold=10&preparedStatementCacheQueries=512&preparedStatementCacheSizeMiB=10
```
Note: Cached prepared statements by the JDBC driver will increase the memory footprint of each JDBC connection in your application and each frontend connection in PgBouncer.

# Running tests
```
./gradlew cleanTest test -info
```

# How it works
PgBouncer has client connections (application :left_right_arrow: PgBouncer) and server connections (PgBouncer :left_right_arrow: PostgreSQL). Prepared statements received with a 'parse' command are stored on the receiving client connection together with a hash of the sql. Parse commands sent to PostgreSQL are modified to have a unique prepared statement name for the server connection. The server connection stores the sql hash and the prepared statement name unique to that server connection.

Whenever a client connection receives a command involving a prepared statement, it does a lookup to get the prepared statement metadata (sql sent by the parse command and the sql hash). The server connection maps this sql hash to the prepared statement name unique to that server connection and modifies the prepared statement name before sending the command to PostgreSQL. If the sql hash cannot be found for the server connection, this means we are on a different server connection than the one we originally issued the parse command on. In this case, a parse command is generated and sent first.

# Related work
An effort is being made to improve Postgres connection scalability:
- [Improving Postgres Connection Scalability: Snapshots](https://techcommunity.microsoft.com/t5/azure-database-for-postgresql/improving-postgres-connection-scalability-snapshots/ba-p/1806462) by Microsoft
- Postgres [mailing list](https://www.postgresql.org/message-id/20200301083601.ews6hz5dduc3w2se%40alap3.anarazel.de) thread
- Postgres 14 [release notes](https://www.postgresql.org/docs/release/14.0/)
  > Improve the speed of computing MVCC visibility snapshots on systems with many CPUs and high session counts (Andres Freund)</br>
    This also improves performance when there are many idle sessions.

