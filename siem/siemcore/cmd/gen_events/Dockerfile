FROM golang:1.23-alpine AS builder

WORKDIR /app
COPY . .

RUN cd siem/siemcore/cmd/gen_events && \
    CGO_ENABLED=0 go build -o /gen_events ./main.go

FROM golang:1.23-alpine

WORKDIR /app
COPY --from=builder /gen_events /app/gen_events

ENV DB_ADDR=siem:8080

CMD ["/app/gen_events"]
