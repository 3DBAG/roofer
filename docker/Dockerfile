FROM ubuntu:24.04 AS builder

ARG JOBS=2
ARG VERSION=unknown

ARG ROOFER_ROOT=/opt/roofer
ARG ROOFER_INSTALL=$ROOFER_ROOT/install
ARG ROOFER_BUILD=$ROOFER_ROOT/build

ENV DEBIAN_FRONTEND=noninteractive
ENV PATH=/opt/conan-venv/bin:$PATH

RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean \
    && apt-get update \
    && apt-get -y install \
      ca-certificates \
      cmake \
      curl \
      g++ \
      gcc \
      git \
      ninja-build \
      python3 \
      python3-venv \
    && apt-get clean  \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/conan-venv \
    && /opt/conan-venv/bin/pip install --upgrade pip \
    && /opt/conan-venv/bin/pip install "conan>=2,<3"

RUN mkdir -p $ROOFER_ROOT/install $ROOFER_ROOT/build
WORKDIR $ROOFER_ROOT

COPY --link ./apps $ROOFER_ROOT/apps
COPY --link ./cmake $ROOFER_ROOT/cmake
COPY --link ./include $ROOFER_ROOT/include
COPY --link ./src $ROOFER_ROOT/src
COPY --link ./CMakeLists.txt $ROOFER_ROOT/CMakeLists.txt
COPY --link ./LICENSE $ROOFER_ROOT/LICENSE
COPY --link ./README.md $ROOFER_ROOT/README.md
COPY --link ./conanfile.py $ROOFER_ROOT/conanfile.py

RUN --mount=type=cache,target=/root/.conan2 \
    conan profile detect --force \
    && conan install . \
      --output-folder=$ROOFER_BUILD \
      --build=missing \
      --settings=build_type=Release \
      --settings=compiler.cppstd=20 \
      --options="&:build_apps=True" \
      --options="&:use_spdlog=True" \
      --options="&:use_val3dity=False" \
      --options="&:build_bindings=False" \
      --options="&:build_testing=False" \
    && cmake -B $ROOFER_BUILD \
      -S $ROOFER_ROOT \
      -DCMAKE_TOOLCHAIN_FILE=$ROOFER_BUILD/conan_toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$ROOFER_INSTALL \
      -DRF_GIT_HASH=$VERSION \
    && cmake --build $ROOFER_BUILD -j $JOBS --target install --config Release


FROM ubuntu:24.04 AS production

ARG ROOFER_ROOT=/opt/roofer

COPY --link --from=builder $ROOFER_ROOT/install/ /opt/roofer/

ENV GDAL_DATA=/opt/roofer/share/gdal
ENV PROJ_DATA=/opt/roofer/share/proj
ENV PATH=/opt/roofer/bin:$PATH

ENTRYPOINT ["roofer"]
