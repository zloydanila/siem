FROM ubuntu:24.04

# Go + deps
RUN apt-get update && apt-get install -y \
  build-essential cmake pkg-config nlohmann-json3-dev ca-certificates curl && \
  rm -rf /var/lib/apt/lists/* && \
  curl -L https://golang.org/dl/go1.23.6.linux-amd64.tar.gz | tar -C /usr/local -xzf -

ENV PATH="/usr/local/go/bin:${PATH}"

WORKDIR /app

# Копируем исходники
COPY siem/ ./siem
COPY siem/siemcore/ ./siemcore
COPY siem/schema.json .

# Build DB Server
RUN cd siem && mkdir build && cd build && cmake .. && make -j$(nproc) db_server && \
  cp db_server ../../

# Build Go UI
RUN cd siemcore && go mod download && go build -ldflags="-s -w" -o ../siemcore ./cmd/siemcore

# Права!
RUN chmod +x db_server siemcore

EXPOSE 8080 8088
CMD ./db_server --port 8080 --schema ./schema.json --data-root ./data & ./siemcore
