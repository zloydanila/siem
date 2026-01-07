FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential cmake pkg-config nlohmann-json3-dev curl ca-certificates \
  golang-go && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY siem/ .

# Build C++ DB
RUN mkdir build && cd build \
  && cmake -DCMAKE_BUILD_TYPE=Release .. \
  && make -j$(nproc)

# Build Go UI
WORKDIR /app/siemcore
COPY siem/siemcore/ .
RUN go mod download || true
RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o ../siemcore ./cmd/siemcore

WORKDIR /app
COPY siem/siemcore/web ./siemcore/web

EXPOSE 8080 8088
CMD sh -c "./build/db_server --port 8080 --schema ./schema.json --data-root ./data & ./siemcore"
