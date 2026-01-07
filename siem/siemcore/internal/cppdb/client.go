package cppdb

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"time"
)

type Client struct {
	Addr    string
	Timeout time.Duration
}

type Response struct {
	Status  string          `json:"status"`
	Message string          `json:"message"`
	Data    json.RawMessage `json:"data"`
	Count   int             `json:"count"`
}

func (c *Client) Do(req any) (Response, error) {
	if c == nil || c.Addr == "" {
		return Response{}, errors.New("nil client or empty Addr")
	}

	dialer := net.Dialer{Timeout: c.Timeout}
	conn, err := dialer.Dial("tcp", c.Addr)
	if err != nil {
		return Response{}, fmt.Errorf("dial %s: %w", c.Addr, err)
	}
	defer conn.Close()

	_ = conn.SetDeadline(time.Now().Add(c.Timeout))

	b, err := json.Marshal(req)
	if err != nil {
		return Response{}, fmt.Errorf("marshal req: %w", err)
	}
	b = append(b, '\n')

	if _, err := conn.Write(b); err != nil {
		return Response{}, fmt.Errorf("write: %w", err)
	}

	r := bufio.NewReader(conn)
	line, err := r.ReadBytes('\n')
	if err != nil {
		return Response{}, fmt.Errorf("read: %w", err)
	}

	fmt.Printf("DB RAW: %q\n", line)

	var resp Response
	if err := json.Unmarshal(trimNL(line), &resp); err != nil {
		return Response{}, fmt.Errorf("json unmarshal: %w, raw: %q", err, line)
	}

	return resp, nil
}

func trimNL(b []byte) []byte {
	for len(b) > 0 && (b[len(b)-1] == '\n' || b[len(b)-1] == '\r') {
		b = b[:len(b)-1]
	}
	return b
}
