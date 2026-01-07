# Мультистейдж сборка
FROM golang:1.23-alpine AS builder
WORKDIR /app
COPY siem/siemcore/go.mod siem/siemcore/go.sum* ./
RUN go mod download
COPY siem/siemcore/ .
RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o siemcore ./cmd/siemcore

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y build-essential cmake pkg-config nlohmann-json3-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY siem/ .
RUN mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)
COPY --from=builder /app/siemcore ./siemcore
EXPOSE 8080 8088
CMD ["sh", "-c", "./build/db_server --port 8080 & ./siemcore"]
