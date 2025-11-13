ARG BOOST_VERSION="1.89.0"
ARG BOOST_VARIANT="release"
ARG LINK="static"

FROM ghcr.io/morphgrid/compiler:${BOOST_VERSION}-${BOOST_VARIANT}-${LINK} AS builder
ARG BOOST_VARIANT
ARG LINK
ARG BOOST_VERSION

WORKDIR /srv

COPY . .
RUN sh scripts/build.sh ${BOOST_VARIANT} ${LINK}

FROM alpine:latest AS runtime
WORKDIR /srv
COPY --from=builder /srv/build/server /usr/local/bin/server

EXPOSE 9000
ENTRYPOINT ["/usr/local/bin/server"]