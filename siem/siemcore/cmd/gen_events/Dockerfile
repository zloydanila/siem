FROM golang:1.23-alpine

WORKDIR /app
COPY . .
COPY --from=builder /app/siemcore /app/siemcore

RUN cd siem/siemcore/cmd/gen_events && \
    CGO_ENABLED=0 go build -o /app/gen_events ./main.go

CMD ["/app/gen_events"]
