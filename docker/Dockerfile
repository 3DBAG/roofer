FROM ubuntu:24.04 AS vcpkg-build

# Set the location for the vcpkg download cache
RUN mkdir -p /var/cache/vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
ENV VCPKG_DOWNLOADS=/var/cache/vcpkg
ENV VCPKG_DEFAULT_TRIPLET=x64-linux
ENV VCPKG_BUILD_TYPE=release

# Install dependencies for building vcpkg and packages
RUN apt-get update \
    && apt-get install -y g++ gcc cmake git curl zip unzip tar openssl ca-certificates \
      pkg-config autoconf-archive autoconf libtool autotools-dev automake

# Clone vcpkg
WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git

# Bootstrap vcpkg
WORKDIR /opt/vcpkg
RUN ./bootstrap-vcpkg.sh


FROM ubuntu:24.04 AS builder
ARG JOBS=2

ARG ROOFER_ROOT=/opt/roofer
ARG ROOFER_INSTALL=$ROOFER_ROOT/install
ARG ROOFER_BUILD=$ROOFER_ROOT/build
# Set the VCPKG_ROOT environment variable so your build system can find it
ENV VCPKG_ROOT=/opt/vcpkg
# Cache the downloads from vcpkg in the builder stage as well
ENV VCPKG_DOWNLOADS=/var/cache/vcpkg
ENV VCPKG_DEFAULT_TRIPLET=x64-linux
ENV VCPKG_BUILD_TYPE=release

RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean \
    && apt-get update \
    && apt-get -y install g++ gcc cmake git curl zip unzip tar openssl ca-certificates \
      pkg-config ninja-build autoconf-archive autoconf libtool bison flex autotools-dev automake linux-libc-dev \
      python3 bison flex \
    && apt-get clean  \
    && rm -rf /var/lib/apt/lists/*

# Copy vcpkg artifacts from the previous stage
COPY --from=vcpkg-build /opt/vcpkg /opt/vcpkg
COPY --from=vcpkg-build /var/cache/vcpkg /var/cache/vcpkg

RUN mkdir -p $ROOFER_ROOT/install $ROOFER_ROOT/build/vcpkg_installed
WORKDIR $ROOFER_ROOT
# We only need .git because of the cmake-git-version-tracking
COPY --link ./.git $ROOFER_ROOT/.git
COPY --link ./apps $ROOFER_ROOT/apps
COPY --link ./cmake $ROOFER_ROOT/cmake
COPY --link ./include $ROOFER_ROOT/include
COPY --link ./rooferpy $ROOFER_ROOT/rooferpy
COPY --link ./src $ROOFER_ROOT/src
COPY --link ./CMakeLists.txt $ROOFER_ROOT/CMakeLists.txt
COPY --link ./CMakePresets.json $ROOFER_ROOT/CMakePresets.json
COPY --link ./LICENSE $ROOFER_ROOT/LICENSE
COPY --link ./README.rst $ROOFER_ROOT/README.rst
# COPY --link ./requirements.txt $ROOFER_ROOT/requirements.txt
COPY --link ./vcpkg.json $ROOFER_ROOT/vcpkg.json

WORKDIR $ROOFER_ROOT
RUN $VCPKG_ROOT/vcpkg x-update-baseline

RUN --mount=target=$ROOFER_BUILD/vcpkg_installed,type=cache cmake \
    --preset vcpkg-minimal \
    -DRF_USE_LOGGER_SPDLOG=ON \
    -DRF_BUILD_APPS=ON \
    -DRF_USE_VAL3DITY=ON \
    -DCMAKE_INSTALL_PREFIX=$ROOFER_INSTALL \
    -S $ROOFER_ROOT \
    -B $ROOFER_BUILD \
    -G Ninja \
    && cmake --build $ROOFER_BUILD -j $JOBS --target install --config Release \
    && cp -r $ROOFER_BUILD/vcpkg_installed/x64-linux/share/gdal $ROOFER_INSTALL/gdal \
    && cp -r $ROOFER_BUILD/vcpkg_installed/x64-linux/share/proj $ROOFER_INSTALL/proj


FROM ubuntu:24.04 AS production
ARG ROOFER_ROOT=/opt/roofer
ARG ROOFER_INSTALL=$ROOFER_ROOT/install
ARG ROOFER_BUILD=$ROOFER_ROOT/build

COPY --link --from=builder $ROOFER_INSTALL/bin /opt/roofer/bin
COPY --link --from=builder $ROOFER_INSTALL/gdal /opt/roofer/share/gdal
COPY --link --from=builder $ROOFER_INSTALL/proj /opt/roofer/share/proj

ENV GDAL_DATA=/opt/roofer/share/gdal
ENV PROJ_DATA=/opt/roofer/share/proj
ENV PATH=/opt/roofer/bin:$PATH
