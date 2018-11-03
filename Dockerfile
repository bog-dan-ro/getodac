FROM debian:testing-slim
MAINTAINER   BogDan Vatra <bogdan@kde.org>

WORKDIR /code
ADD . /code

RUN apt-get update && apt-get -y install \
  g++ \
  libssl-dev \
  libboost-coroutine-dev \
  libboost-program-options-dev \
  libboost-filesystem-dev \
  libboost-system-dev \
  libboost-iostreams-dev \
  libgtest-dev \
  libcurl4-openssl-dev \
  cmake \
  ninja-build \
  git \
  wget \
  xz-utils
