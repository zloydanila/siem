FROM ubuntu:24.04 as builder

RUN apt-get update && apt-get install -y \
  build-essential cmake pkg-config nlohmann-json3-dev ca-certificates curl && \
  curl -L https://golang.org/dl/go1.23.6.linux-amd64.tar.gz | tar -C /usr/local -xzf - && \
  rm -rf /var/lib/apt/lists/*

ENV PATH="/usr/local/go/bin:${PATH}"
WORKDIR /app

COPY siem/ ./siem/
COPY siem/schema.json .

# Собираем DB сервер
RUN cd siem && mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc) && cp db_server ../../

# Собираем UI
RUN cd siem/siemcore && go mod download && go build -ldflags="-s -w" -o ../../ui ./cmd/siemcore && cp -r web ../../

# Runtime
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y curl && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/db_server .
COPY --from=builder /app/ui .
COPY --from=builder /app/web ./web
COPY --from=builder /app/schema.json .

RUN chmod +x db_server ui

EXPOSE 8080 8088

CMD sh -c './db_server --port 8080 --schema /app/schema.json --data-root /app/data & ./ui'
