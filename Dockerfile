FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ \
        make \
        git \
        ca-certificates \
        wget \
    && rm -rf /var/lib/apt/lists/*

ARG CMAKE_VERSION=4.0.1
RUN wget -qO- "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-$(uname -m).tar.gz" \
    | tar xz --strip-components=1 -C /usr/local

WORKDIR /src
