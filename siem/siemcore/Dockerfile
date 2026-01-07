FROM golang:1.23-alpine AS builder
WORKDIR /app
COPY go.mod go.sum* ./
RUN go mod download
COPY . .
RUN ls -la cmd/
RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o siemcore ./cmd/siemcore

FROM alpine:3.20
RUN apk add --no-cache ca-certificates curl
WORKDIR /app
COPY --from=builder /app/siemcore .
COPY web/static ./web/static
COPY web/templates ./web/templates
EXPOSE 8088
CMD ["./siemcore"]
