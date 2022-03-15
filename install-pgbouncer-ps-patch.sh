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
for file in $MERGEFILES
do
   echo Merging pgbouncer-ps changes to: $PGDIR/$file
   patch -d $PGDIR -f -b -p1 < $PATCHDIR/$file.diff || patchstatus=1
done

# copy pgbouncer-ps source files
mkdir -p $PGDIR/images
NEWFILES="\
  include/messages.h\
  include/ps.h\
  include/common/uthash.h\
  src/messages.c\
  src/ps.c\
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
   echo "      - last tested with pgbouncer v1.14 (June 2020)"
   echo "      - Try getting pgbouncer with: git clone https://github.com/pgbouncer/pgbouncer.git --branch \"pgbouncer_1_14_0\""
   echo "Status: pgbouncer-ps-patch merge FAILED"
else
   echo "Status: pgbouncer-ps-patch merge SUCEEDED"
fi
exit $patchstatus
