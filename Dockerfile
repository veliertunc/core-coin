FROM ubuntu:24.04 AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake g++ make libssl-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
COPY CMakeLists.txt ./
COPY include/ include/
COPY src/ src/
RUN cmake -S . -B /build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build /build --target corecoin-cli --parallel

FROM ubuntu:24.04 AS runtime
RUN apt-get update \
    && apt-get install -y --no-install-recommends libssl3t64 libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --home-dir /data corecoin \
    && mkdir -p /data \
    && chown corecoin:corecoin /data
COPY --from=build /build/src/cli/corecoin-cli /usr/local/bin/corecoin-cli
USER corecoin
WORKDIR /data
ENTRYPOINT ["corecoin-cli"]
CMD ["--shell", "/data/corecoin.chain"]
