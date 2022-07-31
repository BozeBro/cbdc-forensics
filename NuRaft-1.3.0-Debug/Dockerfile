FROM ubuntu:20.04
# set non-interactive shell
ENV DEBIAN_FRONTEND noninteractive
RUN apt update && \
  apt-get -y install \
  build-essential \
  cmake \
  openssl \
  libssl-dev \
  libz-dev \
  git \
  libjsoncpp-dev

# Set working directory
WORKDIR /NuRaft-1.3.0-Debug

# Copy source
COPY . .
RUN mkdir /data
# Use the raft that is already installed
RUN ./prepare.sh && \
  rm -rf build && \
  mkdir build && \
  cd build && \
  cmake ../ && \
  make -j$(nproc)
