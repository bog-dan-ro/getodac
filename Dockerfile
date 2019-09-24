FROM debian:buster-slim
MAINTAINER  BogDan Vatra <bogdan@kde.org>

WORKDIR /code
ADD . /code

RUN apt-get update && apt-get -y install \
  g++ \
  libboost-coroutine-dev \
  libboost-filesystem-dev \
  libboost-iostreams-dev \
  libboost-log-dev \
  libboost-program-options-dev \
  libboost-system-dev \
  libcurl4-openssl-dev \
  libgtest-dev \
  libssl-dev \
  cmake \
  ninja-build \
  git \
  wget \
  xz-utils
