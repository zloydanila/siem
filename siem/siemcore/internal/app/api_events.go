package app

import (
	"encoding/base64"
	"encoding/csv"
	"encoding/json"
	"errors"
	"net/http"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"
)

type Event struct {
	ID        string `json:"_id"`
	AgentID   string `json:"agentid"`
	PacketTS  string `json:"packettimestamp"`
	TS        string `json:"timestamp"`
	Hostname  string `json:"hostname"`
	Source    string `json:"source"`
	EventType string `json:"eventtype"`
	Severity  string `json:"severity"`
	User      string `json:"user"`
	Process   string `json:"process"`
	Command   string `json:"command"`
	RawLog    string `json:"rawlog"`
	SrcIP     string `json:"srcip,omitempty"`
	DstIP     string `json:"dstip,omitempty"`
	SrcPort   int    `json:"srcport,omitempty"`
	DstPort   int    `json:"dstport,omitempty"`
	Protocol  string `json:"protocol,omitempty"`
	Outcome   string `json:"outcome,omitempty"`
	RuleID    string `json:"ruleid,omitempty"`
	RuleName  string `json:"rulename,omitempty"`
}

type eventsPage struct {
	Items      []Event
	HasMore    bool
	NextCursor string
}

type eventsQuery struct {
	Limit    int
	Cursor   string
	Q        string
	Regex    bool
	User     string
	Host     string
	Type     string
	Severity string
	Process  string
	Agent    string
	Source   string
}

func (a *App) handleAPIEvents(w http.ResponseWriter, r *http.Request) {
	q, err := parseEventsQuery(r, 500, 100)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]any{"status": "error", "message": err.Error()})
		return
	}

	events, err := a.loadAllEvents()
	if err != nil {
		writeJSON(w, http.StatusBadGateway, map[string]any{"status": "error", "message": err.Error()})
		return
	}

	page, err := queryEvents(events, q)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]any{"status": "error", "message": err.Error()})
		return
	}

	writeJSON(w, http.StatusOK, map[string]any{
		"status":      "success",
		"data":        page.Items,
		"count":       len(page.Items),
		"has_more":    page.HasMore,
		"next_cursor": page.NextCursor,
	})
}

func (a *App) handleAPIEventsByID(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/events/")
	id = strings.TrimSpace(id)
	if id == "" {
		writeJSON(w, http.StatusBadRequest, map[string]any{"status": "error", "message": "empty id"})
		return
	}

	events, err := a.loadAllEvents()
	if err != nil {
		writeJSON(w, http.StatusBadGateway, map[string]any{"status": "error", "message": err.Error()})
		return
	}
	for _, e := range events {
		if e.ID == id {
			writeJSON(w, http.StatusOK, map[string]any{"status": "success", "data": e})
			return
		}
	}
	writeJSON(w, http.StatusNotFound, map[string]any{"status": "error", "message": "not found"})
}

func (a *App) handleAPIEventsCSV(w http.ResponseWriter, r *http.Request) {
	q, err := parseEventsQuery(r, 50000, 50000)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	events, err := a.loadAllEvents()
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}

	page, err := queryEvents(events, q)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	w.Header().Set("Content-Type", "text/csv; charset=utf-8")
	w.Header().Set("Content-Disposition", `attachment; filename="events.csv"`)
	w.WriteHeader(http.StatusOK)

	cw := csv.NewWriter(w)
	_ = cw.Write([]string{
		"_id", "agentid", "packettimestamp", "timestamp", "hostname", "source",
		"eventtype", "severity", "user", "process", "command", "rawlog",
	})
	for _, e := range page.Items {
		_ = cw.Write([]string{
			e.ID, e.AgentID, e.PacketTS, e.TS, e.Hostname, e.Source,
			e.EventType, e.Severity, e.User, e.Process, e.Command, e.RawLog,
		})
	}
	cw.Flush()
}

func (a *App) handleAPIEventsJSONExport(w http.ResponseWriter, r *http.Request) {
	q, err := parseEventsQuery(r, 50000, 50000)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]any{"status": "error", "message": err.Error()})
		return
	}

	events, err := a.loadAllEvents()
	if err != nil {
		writeJSON(w, http.StatusBadGateway, map[string]any{"status": "error", "message": err.Error()})
		return
	}

	page, err := queryEvents(events, q)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]any{"status": "error", "message": err.Error()})
		return
	}

	w.Header().Set("Content-Disposition", `attachment; filename="events.json"`)
	writeJSON(w, http.StatusOK, map[string]any{"status": "success", "data": page.Items, "count": len(page.Items)})
}

func parseEventsQuery(r *http.Request, maxLimit int, defLimit int) (eventsQuery, error) {
	limit := defLimit
	if s := r.URL.Query().Get("limit"); s != "" {
		n, err := strconv.Atoi(s)
		if err != nil || n <= 0 {
			return eventsQuery{}, errors.New("invalid limit")
		}
		if n > maxLimit {
			n = maxLimit
		}
		limit = n
	}

	q := strings.TrimSpace(r.URL.Query().Get("q"))
	if len(q) > 256 {
		return eventsQuery{}, errors.New("q too long")
	}
	re := r.URL.Query().Get("re") == "1"

	return eventsQuery{
		Limit:    limit,
		Cursor:   strings.TrimSpace(r.URL.Query().Get("cursor")),
		Q:        q,
		Regex:    re,
		User:     strings.TrimSpace(r.URL.Query().Get("user")),
		Host:     strings.TrimSpace(r.URL.Query().Get("host")),
		Type:     strings.TrimSpace(r.URL.Query().Get("type")),
		Severity: strings.TrimSpace(r.URL.Query().Get("severity")),
		Process:  strings.TrimSpace(r.URL.Query().Get("process")),
		Agent:    strings.TrimSpace(r.URL.Query().Get("agent")),
		Source:   strings.TrimSpace(r.URL.Query().Get("source")),
	}, nil
}

func queryEvents(all []Event, q eventsQuery) (eventsPage, error) {
	events := make([]Event, 0, len(all))
	events = append(events, all...)

	sort.Slice(events, func(i, j int) bool {
		if events[i].TS == events[j].TS {
			return events[i].ID > events[j].ID
		}
		return events[i].TS > events[j].TS
	})

	var rx *regexp.Regexp
	if q.Q != "" && q.Regex {
		r, err := regexp.Compile(q.Q)
		if err != nil {
			return eventsPage{}, errors.New("bad regex")
		}
		rx = r
	}

	afterTS, afterID := "", ""
	if q.Cursor != "" {
		ts, id, err := decodeCursor(q.Cursor)
		if err != nil {
			return eventsPage{}, errors.New("bad cursor")
		}
		afterTS, afterID = ts, id
	}

	match := func(e Event) bool {
		if q.User != "" && !eqFold(e.User, q.User) {
			return false
		}
		if q.Host != "" && !eqFold(e.Hostname, q.Host) {
			return false
		}
		if q.Type != "" && !eqFold(e.EventType, q.Type) {
			return false
		}
		if q.Severity != "" && !eqFold(e.Severity, q.Severity) {
			return false
		}
		if q.Process != "" && !eqFold(e.Process, q.Process) {
			return false
		}
		if q.Agent != "" && !eqFold(e.AgentID, q.Agent) {
			return false
		}
		if q.Source != "" && !eqFold(e.Source, q.Source) {
			return false
		}

		if q.Q == "" {
			return true
		}

		hay := strings.Join([]string{
			e.ID, e.AgentID, e.PacketTS, e.TS, e.Hostname, e.Source,
			e.EventType, e.Severity, e.User, e.Process, e.Command, e.RawLog,
		}, "\n")

		if rx != nil {
			return rx.MatchString(hay)
		}
		return strings.Contains(strings.ToLower(hay), strings.ToLower(q.Q))
	}

	after := func(e Event) bool {
		if afterTS == "" && afterID == "" {
			return true
		}
		if e.TS < afterTS {
			return true
		}
		if e.TS == afterTS && e.ID < afterID {
			return true
		}
		return false
	}

	out := make([]Event, 0, q.Limit+1)
	for _, e := range events {
		if !after(e) || !match(e) {
			continue
		}
		out = append(out, e)
		if len(out) >= q.Limit+1 {
			break
		}
	}

	hasMore := len(out) > q.Limit
	if hasMore {
		out = out[:q.Limit]
	}

	nextCursor := ""
	if hasMore && len(out) > 0 {
		last := out[len(out)-1]
		nextCursor = encodeCursor(last.TS, last.ID)
	}

	return eventsPage{Items: out, HasMore: hasMore, NextCursor: nextCursor}, nil
}

func eqFold(a, b string) bool {
	return strings.EqualFold(strings.TrimSpace(a), strings.TrimSpace(b))
}

func encodeCursor(ts, id string) string {
	raw := ts + "|" + id
	return base64.RawURLEncoding.EncodeToString([]byte(raw))
}

func decodeCursor(cur string) (string, string, error) {
	b, err := base64.RawURLEncoding.DecodeString(cur)
	if err != nil {
		return "", "", err
	}
	parts := strings.SplitN(string(b), "|", 2)
	if len(parts) != 2 {
		return "", "", errors.New("bad cursor")
	}
	return parts[0], parts[1], nil
}

func (a *App) loadAllEvents() ([]Event, error) {
	req := map[string]any{
		"database":   "siem",
		"collection": "securityevents",
		"operation":  "find",
		"query":      map[string]any{},
	}

	resp, err := a.cfg.DBClient.Do(req)
	if err != nil {
		return nil, err
	}
	if resp.Status != "success" {
		return nil, errors.New(resp.Message)
	}

	var events []Event
	if len(resp.Data) == 0 {
		return []Event{}, nil
	}
	if err := json.Unmarshal(resp.Data, &events); err != nil {
		return nil, err
	}
	return events, nil
}

func parseTS(s string) (time.Time, bool) {
	s = strings.TrimSpace(s)
	if s == "" {
		return time.Time{}, false
	}
	t, err := time.Parse("2006-01-02T150405Z", s)
	if err == nil {
		return t, true
	}
	t, err = time.Parse(time.RFC3339, s)
	if err == nil {
		return t, true
	}
	return time.Time{}, false
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(v)
}
