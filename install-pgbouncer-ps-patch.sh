USAGE="Usage: $0 <pgbouncer dir>"
usage() {
   echo $USAGE
   exit 1
}

PGDIR=$1
[ -z "$PGDIR" ] && usage

PATCHDIR=$(pwd)
patchstatus=0

# Patch each modified file
MERGEFILES="\
  Makefile\
  configure.ac\
  etc/pgbouncer.ini\
  include/common/postgres_compat.h\
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
for file in $MERGEFILES
do
   echo Merging pgbouncer-ps changes to: $PGDIR/$file
   patch -d $PGDIR -f -b -p1 < $PATCHDIR/$file.diff || patchstatus=1
done

# copy pgbouncer-ps source files
NEWFILES="\
  include/protocol_message.h\
  include/prepared_statement.h\
  include/common/uthash.h\
  src/common/numutils.c\
  src/protocol_message.c\
  src/prepared_statement.c\
  test/test_prepared_statement.py\
  "
echo -n "copying pgbouncer-ps files: "
for file in $NEWFILES
do
   echo -n "$file "
   cp $PATCHDIR/$file $PGDIR/$file || patchstatus=1
done
echo

if [ $patchstatus -eq 1 ]; then
   echo "Failures encountered during merge of pgbouncer-ps with pgbouncer."
   echo "See error messages above."
   echo "Possible causes: "
   echo "   pgbouncer-ps-patch already installed in target directory?"
   echo "   new version of pgbouncer with changed source files that can't be patched?"
   echo "      - last tested with pgbouncer v1.19.0 (May 2023)"
   echo "      - Try getting pgbouncer with: git clone https://github.com/pgbouncer/pgbouncer.git --branch \"pgbouncer_1_19_0\""
   echo "Status: pgbouncer-ps-patch merge FAILED"
else
   echo "Status: pgbouncer-ps-patch merge SUCEEDED"
fi
exit $patchstatus
