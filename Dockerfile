FROM debian:testing-slim
MAINTAINER   BogDan Vatra <bogdan@kde.org>

WORKDIR /code
ADD . /code

RUN apt-get update && apt-get -y install \
  g++ \
  libssl-dev \
  libboost-all-dev \
  libgtest-dev \
  libcurl4-openssl-dev \
  cmake \
  ninja-build \
  git \
  wget \
  xz-utils
