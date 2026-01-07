package app

import (
	"net/http"
	"sort"
	"strings"
	"time"
)

type KV struct {
	Key   string `json:"key"`
	Value any    `json:"value"`
}

type Dashboard struct {
	ActiveAgents    []KV    `json:"active_agents"`
	EventsPerHour24 []int   `json:"events_per_hour_24"`
	Hosts24h        []KV    `json:"hosts_24h"`
	LastLogins      []Event `json:"last_logins"`
	Severity24h     []KV    `json:"severity_24h"`
	TopEventTypes   []KV    `json:"top_event_types"`
	TopProcesses24h []KV    `json:"top_processes_24h"`
	TopUsers24h     []KV    `json:"top_users_24h"`
}

func (a *App) handleAPIDashboard(w http.ResponseWriter, r *http.Request) {
	events, err := a.loadAllEvents()
	if err != nil {
		writeJSON(w, http.StatusBadGateway, map[string]any{"status": "error", "message": err.Error()})
		return
	}

	cut := time.Now().Add(-24 * time.Hour)
	events = filterLast24h(events, cut)

	d := buildDashboard(events)
	writeJSON(w, http.StatusOK, d)
}

func filterLast24h(events []Event, cut time.Time) []Event {
	out := make([]Event, 0, len(events))
	for _, e := range events {
		t, ok := parseTS(e.TS)
		if !ok {
			continue
		}
		if !t.Before(cut) {
			out = append(out, e)
		}
	}
	return out
}

func buildDashboard(events []Event) Dashboard {
	eventsPerHour := make([]int, 24)

	hosts := map[string]int{}
	severity := map[string]int{}
	topTypes := map[string]int{}
	topProc := map[string]int{}
	topUsers := map[string]int{}
	activeAgents := map[string]string{}

	lastLogins := make([]Event, 0, 10)

	for _, e := range events {

		if t, ok := parseTS(e.TS); ok {
			h := t.UTC().Hour()
			if h >= 0 && h < 24 {
				eventsPerHour[h]++
			}
		}

		if e.Hostname != "" {
			hosts[e.Hostname]++
		}
		if e.Severity != "" {
			severity[e.Severity]++
		}
		if e.EventType != "" {
			topTypes[e.EventType]++
		}
		if e.Process != "" {
			topProc[e.Process]++
		}
		if e.User != "" {
			topUsers[e.User]++
		}
		if e.AgentID != "" && e.PacketTS != "" {
			if prev, ok := activeAgents[e.AgentID]; !ok || e.PacketTS > prev {
				activeAgents[e.AgentID] = e.PacketTS
			}
		}

		et := strings.ToLower(strings.TrimSpace(e.EventType))
		if et == "userlogin" || et == "userlogin_failed" || et == "login" {
			lastLogins = append(lastLogins, e)
		}
	}

	sort.Slice(lastLogins, func(i, j int) bool { return lastLogins[i].TS > lastLogins[j].TS })
	if len(lastLogins) > 10 {
		lastLogins = lastLogins[:10]
	}

	return Dashboard{
		ActiveAgents:    mapToKVStr(activeAgents),
		EventsPerHour24: eventsPerHour,
		Hosts24h:        topN(mapToKVInt(hosts), 10),
		LastLogins:      lastLogins,
		Severity24h:     mapToKVInt(severity),
		TopEventTypes:   topN(mapToKVInt(topTypes), 10),
		TopProcesses24h: topN(mapToKVInt(topProc), 10),
		TopUsers24h:     topN(mapToKVInt(topUsers), 10),
	}
}

func mapToKVInt(m map[string]int) []KV {
	out := make([]KV, 0, len(m))
	for k, v := range m {
		out = append(out, KV{Key: k, Value: v})
	}
	sort.Slice(out, func(i, j int) bool {
		return out[i].Value.(int) > out[j].Value.(int)
	})
	return out
}

func mapToKVStr(m map[string]string) []KV {
	out := make([]KV, 0, len(m))
	for k, v := range m {
		out = append(out, KV{Key: k, Value: v})
	}
	sort.Slice(out, func(i, j int) bool {
		return out[i].Key < out[j].Key
	})
	return out
}

func topN(kv []KV, n int) []KV {
	if len(kv) <= n {
		return kv
	}
	return kv[:n]
}
