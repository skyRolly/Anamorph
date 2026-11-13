FROM debian:trixie-slim@sha256:62bd423aeee912639ca8ee681c2b762cb3e25137e0a92b20be455898a9f034ad

# Use signed Debian packages and the current session's CA bundle. The bundle
# stays in a BuildKit secret mount rather than an image layer.
RUN --mount=type=secret,id=system_ca,mode=0444,required=true \
    sed -i 's|http://deb.debian.org|https://deb.debian.org|g' /etc/apt/sources.list.d/debian.sources && \
    apt-get -o Acquire::https::CaInfo=/run/secrets/system_ca update && \
    DEBIAN_FRONTEND=noninteractive apt-get -o Acquire::https::CaInfo=/run/secrets/system_ca \
      install -y --no-install-recommends \
      build-essential cmake ninja-build pkg-config ccache git ca-certificates \
      python3 curl unzip xvfb xauth \
      libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev \
      libfreetype-dev libfontconfig1-dev \
      libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxi-dev \
      libxinerama-dev libxrandr-dev libxrender-dev \
      libglu1-mesa-dev mesa-common-dev libegl-dev libgl1-mesa-dri && \
    rm -rf /var/lib/apt/lists/* && \
    groupadd --gid 1000 developer && \
    useradd --uid 1000 --gid 1000 --create-home developer

ENV CMAKE_BUILD_PARALLEL_LEVEL=4 CCACHE_DIR=/workspace/.cloud-setup/ccache CCACHE_MAXSIZE=1G
USER developer
WORKDIR /workspace
