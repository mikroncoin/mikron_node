FROM ubuntu:16.04

RUN apt-get update -qq && apt-get install -yqq \
    build-essential \
    cmake \
    g++ \
    wget \
    libssl-dev

ENV BOOST_ROOT=/tmp/boost_install
ADD ci /tmp/ci
RUN /tmp/ci/bootstrap_boost.sh -m

ADD ./ /tmp/src

ARG NETWORK=live
RUN echo ${NETWORK} > /etc/mikron-network

RUN mkdir /tmp/build
WORKDIR /tmp/build

RUN cmake /tmp/src -DBOOST_ROOT=${BOOST_ROOT} -DACTIVE_NETWORK=rai_${NETWORK}_network
RUN cmake --build . --config Release --target mikron_node

FROM ubuntu:16.04
COPY --from=0 /etc/mikron-network /etc
COPY --from=0 /tmp/build/mikron_node /usr/bin
COPY docker/node/entry.sh /entry.sh
COPY docker/node/config /usr/share/raiblocks/config
RUN chmod +x /entry.sh
CMD ["/bin/bash",  "/entry.sh"]
