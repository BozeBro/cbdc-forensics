FROM ubuntu:20.04

# set non-interactive shell
ENV DEBIAN_FRONTEND noninteractive

# install base packages
RUN apt update && \
    apt install -y \
      build-essential \
      wget \
      cmake \
      libgtest-dev \
      libgmock-dev \
      net-tools \
      lcov \
      git

# Args
ARG CMAKE_BUILD_TYPE="Debug"
ARG LEVELDB_VERSION="1.22"
ARG NURAFT_VERSION="1.3.0"

# Install LevelDB
RUN wget https://github.com/google/leveldb/archive/${LEVELDB_VERSION}.tar.gz && \
    tar xzvf ${LEVELDB_VERSION}.tar.gz && \
    rm -f ${LEVELDB_VERSION}.tar.gz && \
    cd leveldb-${LEVELDB_VERSION} && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DLEVELDB_BUILD_TESTS=0 -DLEVELDB_BUILD_BENCHMARKS=0 -DBUILD_SHARED_LIBS=0 . && \
    make -j$(nproc) && \
    make install

# Set working directory
WORKDIR /opt/tx-processor

# Copy source
COPY . .

# Benedict 
# Downloading NuRaft into directory instead of grabbing online
# Use the raft that is already installed
RUN cd "./NuRaft-${NURAFT_VERSION}-${CMAKE_BUILD_TYPE}" && \
    # wget https://github.com/eBay/NuRaft/archive/v${NURAFT_VERSION}.tar.gz && \
    #tar xzvf v${NURAFT_VERSION}.tar.gz && \
    # rm v${NURAFT_VERSION}.tar.gz && \
    ./prepare.sh && \
    rm -rf build && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DDISABLE_SSL=1 .. && \
    make -j$(nproc) static_lib && \
    cp libnuraft.a /usr/local/lib && \
    cp -r ../include/libnuraft /usr/local/include

# Update submodules and run configure.sh
RUN git submodule init && git submodule update

# Build binaries
RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} .. && \
    make -j$(nproc)
