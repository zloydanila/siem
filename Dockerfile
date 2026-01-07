# === Builder Stage: Go UI ===
FROM golang:1.23-alpine AS go-builder
WORKDIR /app
COPY siem/siemcore/go.mod ./
RUN go mod download 2>/dev/null || true
COPY siem/siemcore/ .
RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o siemcore ./cmd/siemcore

# === Builder Stage: C++ DB ===
FROM ubuntu:24.04 AS cpp-builder
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential cmake pkg-config nlohmann-json3-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY siem/ .
RUN mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)

# === Runtime Stage ===
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /app

# Copy DB binary
COPY --from=cpp-builder /app/build/db_server ./db_server
COPY --from=cpp-builder /app/schema.json ./schema.json

# Copy Go binary
COPY --from=go-builder /app/siemcore ./siemcore
COPY --from=go-builder /app/web ./web

# Create data directory
RUN mkdir -p data

EXPOSE 8080 8088

# Start both services
CMD ["sh", "-c", "./db_server --port 8080 --schema ./schema.json --data-root ./data & ./siemcore"]
