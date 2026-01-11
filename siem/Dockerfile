FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
  build-essential cmake pkg-config nlohmann-json3-dev curl && \
  rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY siem/ /app

RUN mkdir build && cd build && \
  cmake -DCMAKE_BUILD_TYPE=Release .. && \
  make -j$(nproc)

EXPOSE 8080

CMD ["./build/db_server", "--port", "8080", "--schema", "/app/schema.json", "--data-root", "/app/data"]
