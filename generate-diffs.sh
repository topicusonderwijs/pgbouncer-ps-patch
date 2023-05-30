#!/bin/bash

if [ ! -d "$1" ] || [ ! -f "$1/include/bouncer.h" ]; then
  "'$1' is not a PgBouncer source directory"
  exit
fi

SRC_DIR=$1
DST_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

MERGEFILES="\
  Makefile\
  configure.ac\
  etc/pgbouncer.ini\
  include/bouncer.h\
  include/objects.h\
  include/sbuf.h\
  src/admin.c\
  src/client.c\
  src/main.c\
  src/objects.c\
  src/sbuf.c\
  src/server.c\
  src/stats.c\
  "

NEWFILES="\
  include/protocol_message.h\
  include/prepared_statement.h\
  include/common/uthash.h\
  src/protocol_message.c\
  src/prepared_statement.c\
  test/prepared_statement/test.ini\
  test/test_prepared_statement.py\
  "

cd $SRC_DIR

for file in $MERGEFILES
do
  echo Creating patch for changes to: $file
  dir=$(dirname $file)
  if [ "$dir" != "." ]; then
    mkdir -p $DST_DIR/$dir
  fi
  git diff tags/pgbouncer_1_19_0 $file > ${DST_DIR}/$file.diff
done

echo -n "Copying files: "
for file in $NEWFILES
do
  echo -n "$file "
  dir=$(dirname $file)
  if [ "$dir" != "." ]; then
    mkdir -p $DST_DIR/$dir
  fi
  cp $file $DST_DIR/$file
done
echo
