FROM ubuntu:24.04

# Устанавливаем Go правильно + зависимости
RUN apt-get update && apt-get install -y \
  build-essential cmake pkg-config nlohmann-json3-dev ca-certificates curl && \
  rm -rf /var/lib/apt/lists/* && \
  curl -L https://golang.org/dl/go1.23.6.linux-amd64.tar.gz | tar -C /usr/local -xzf -

ENV PATH="/usr/local/go/bin:${PATH}"

WORKDIR /app
COPY siem/ ./siem

# Build DB Server
RUN cd siem && mkdir build && cd build && \
  cmake -DCMAKE_BUILD_TYPE=Release .. && \
  make -j$(nproc) db_server

# Build Go UI
COPY siem/siemcore/ ./siemcore/
WORKDIR /app/siemcore
RUN go mod download && \
  CGO_ENABLED=0 go build -ldflags="-s -w" -o ../siemcore ./cmd/siemcore

WORKDIR /app
COPY siem/schema.json .
EXPOSE 8080 8088
CMD ./siem/build/db_server --port 8080 --schema ./schema.json --data-root ./data & ./siemcore
