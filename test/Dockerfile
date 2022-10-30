FROM centos:7 AS builder

RUN yum -y install libevent-devel openssl-devel python-devel libtool git patch make

WORKDIR /usr/src

RUN git clone https://github.com/pgbouncer/pgbouncer.git --branch "pgbouncer_1_17_0"

COPY . install-pgbouncer-ps-patch.sh pgbouncer-ps/

RUN \
  cd pgbouncer-ps \
  && ./install-pgbouncer-ps-patch.sh ../pgbouncer \
  && cd ../pgbouncer \
  && git submodule init \
  && git submodule update \
  && ./autogen.sh \
  && if [ "x$(uname -m)" = "xaarch64" ]; then args="--build=x86_64-linux-gnu --host=aarch64-linux-gnu" ;fi\
  && ./configure ${args} ... \
  && make -W doc/pgbouncer.1 -W doc/pgbouncer.5 \
  && cp pgbouncer /usr/local/bin/


FROM centos:7
RUN yum -y install libevent openssl
RUN useradd -ms /bin/bash pgbouncer
COPY --from=builder /usr/src/pgbouncer/pgbouncer /usr/local/bin
USER pgbouncer
