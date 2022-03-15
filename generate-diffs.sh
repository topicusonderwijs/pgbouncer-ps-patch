#!/bin/bash

if [ ! -d "$1" ] || [ ! -f "$1/include/bouncer.h" ]; then
  "'$1' is not a PgBouncer source directory"
  exit
fi

SRC_DIR=$1
DST_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

MERGEFILES="\
  Makefile\
  etc/pgbouncer.ini\
  src/client.c\
  src/main.c\
  src/objects.c\
  src/sbuf.c\
  src/server.c\
  src/stats.c\
  include/bouncer.h\
  include/proto.h\
  include/sbuf.h\
  "

NEWFILES="\
  include/messages.h\
  include/ps.h\
  include/common/uthash.h\
  src/messages.c\
  src/ps.c\
  "

cd $SRC_DIR

for file in $MERGEFILES
do
  echo Creating patch for changes to: $file
  dir=$(dirname $file)
  if [ "$dir" != "." ]; then
    mkdir -p $DST_DIR/$dir
  fi
  git diff tags/pgbouncer_1_14_0 $file > ${DST_DIR}/$file.diff
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
