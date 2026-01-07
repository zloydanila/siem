package main

import (
	"fmt"
	"math/rand"
	"net"
	"os"
	"strings"
	"time"

	"siemcore/internal/cppdb"
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


	SrcIP    string `json:"srcip"`
	DstIP    string `json:"dstip"`
	SrcPort  int    `json:"srcport"`
	DstPort  int    `json:"dstport"`
	Protocol string `json:"protocol"`
	Outcome  string `json:"outcome"`
	RuleID   string `json:"ruleid"`
	RuleName string `json:"rulename"`
}

func getenv(k, def string) string {
	v := strings.TrimSpace(os.Getenv(k))
	if v == "" {
		return def
	}
	return v
}

func ts(t time.Time) string { return t.UTC().Format("2006-01-02T150405Z") }

type weightedItem[T any] struct {
	val    T
	weight int
}

func pickWeighted[T any](r *rand.Rand, items []weightedItem[T]) T {
	total := 0
	for _, it := range items {
		if it.weight > 0 {
			total += it.weight
		}
	}
	x := r.Intn(total)
	for _, it := range items {
		if it.weight <= 0 {
			continue
		}
		if x < it.weight {
			return it.val
		}
		x -= it.weight
	}
	return items[len(items)-1].val
}

func randIPInCIDR(r *rand.Rand, cidr string) string {
	_, ipnet, err := net.ParseCIDR(cidr)
	if err != nil {
		return "10.0.0.1"
	}
	ip := ipnet.IP.To4()
	mask := ipnet.Mask
	base := uint32(ip[0])<<24 | uint32(ip[1])<<16 | uint32(ip[2])<<8 | uint32(ip[3])
	m := uint32(mask[0])<<24 | uint32(mask[1])<<16 | uint32(mask[2])<<8 | uint32(mask[3])

	
	hostBits := ^m
	if hostBits <= 2 {
		return ip.String()
	}
	n := uint32(r.Intn(int(hostBits-2))) + 1

	v := (base & m) | (n & hostBits)
	out := net.IPv4(byte(v>>24), byte(v>>16), byte(v>>8), byte(v)).String()
	return out
}

func main() {
	r := rand.New(rand.NewSource(time.Now().UnixNano()))

	dbAddr := getenv("DB_ADDR", "127.0.0.1:8080")
	db := &cppdb.Client{Addr: dbAddr, Timeout: 3 * time.Second}

	users := []string{"alice", "bob", "charlie", "dave", "eve", "mallory", "trent", "peggy", "victor", "oscar"}
	hosts := []string{"ws-01", "ws-02", "ws-03", "srv-auth", "srv-db", "vpn-gw", "k8s-node-1"}
	procs := []string{"sshd", "sudo", "nginx", "postgres", "dockerd", "cron", "bash", "python", "java"}
	sources := []string{"syslog", "auditd", "app"}


	types := []weightedItem[string]{
		{val: "raw", weight: 40},
		{val: "process_start", weight: 30},
		{val: "userlogin", weight: 20},
		{val: "userlogin_failed", weight: 10},
	}

	sevs := []weightedItem[string]{
		{val: "low", weight: 60},
		{val: "medium", weight: 30},
		{val: "high", weight: 10},
	}

	protocols := []weightedItem[string]{
		{val: "tcp", weight: 85},
		{val: "udp", weight: 15},
	}

	type rule struct{ id, name string }
	rulesByType := map[string][]weightedItem[rule]{
		"userlogin": {
			{val: rule{"AUTH-0001", "Successful login"}, weight: 100},
		},
		"userlogin_failed": {
			{val: rule{"AUTH-1001", "Failed login"}, weight: 70},
			{val: rule{"AUTH-2001", "Brute-force suspected"}, weight: 30},
		},
		"process_start": {
			{val: rule{"PROC-1001", "Process start"}, weight: 80},
			{val: rule{"PROC-9001", "Suspicious process start"}, weight: 20},
		},
		"raw": {
			{val: rule{"RAW-0001", "Raw log"}, weight: 100},
		},
	}

	
	srcCIDRs := []string{"10.0.0.0/8", "192.168.0.0/16"}
	dstCIDRs := []string{"10.0.0.0/8", "172.16.0.0/12"}

	commonDstPorts := []weightedItem[int]{
		{val: 22, weight: 20},
		{val: 53, weight: 5},
		{val: 80, weight: 15},
		{val: 443, weight: 20},
		{val: 5432, weight: 10},
		{val: 8080, weight: 10},
		{val: 3389, weight: 5},
		{val: 9200, weight: 5},
		{val: 27017, weight: 5},
		{val: 0, weight: 5},
	}

	now := time.Now()
	total := 500

	ok := 0
	for i := 0; i < total; i++ {
		t := now.Add(-time.Duration(r.Intn(24*60)) * time.Minute)

		u := users[r.Intn(len(users))]
		h := hosts[r.Intn(len(hosts))]
		p := procs[r.Intn(len(procs))]
		et := pickWeighted(r, types)
		sev := pickWeighted(r, sevs)
		src := sources[r.Intn(len(sources))]
		proto := pickWeighted(r, protocols)

		outcome := "unknown"
		if et == "userlogin" {
			outcome = "success"
		} else if et == "userlogin_failed" {
			outcome = "failure"
		}

		ru := pickWeighted(r, rulesByType[et])

		srcIP := randIPInCIDR(r, srcCIDRs[r.Intn(len(srcCIDRs))])
		dstIP := randIPInCIDR(r, dstCIDRs[r.Intn(len(dstCIDRs))])

		srcPort := 1024 + r.Intn(65535-1024)
		dstPort := pickWeighted(r, commonDstPorts)
		if dstPort == 0 {
			dstPort = 1024 + r.Intn(65535-1024)
		}
		
		if dstPort == 53 && proto == "tcp" {
			proto = "udp"
		}

		raw := fmt.Sprintf(
			"type=%s user=%s host=%s proc=%s src=%s:%d dst=%s:%d proto=%s rule=%s msg=test-event #%d",
			strings.ToUpper(et), u, h, p, srcIP, srcPort, dstIP, dstPort, proto, ru.id, i,
		)

		e := Event{
			ID:        fmt.Sprintf("test-%d-%d", time.Now().UnixNano(), i),
			AgentID:   fmt.Sprintf("agent-%03d", 1+r.Intn(7)),
			PacketTS:  ts(t.Add(time.Duration(r.Intn(3)) * time.Second)),
			TS:        ts(t),
			Hostname:  h,
			Source:    src,
			EventType: et,
			Severity:  sev,
			User:      u,
			Process:   p,
			Command:   fmt.Sprintf("/usr/bin/%s --test", p),
			RawLog:    raw,

			SrcIP:    srcIP,
			DstIP:    dstIP,
			SrcPort:  srcPort,
			DstPort:  dstPort,
			Protocol: proto,
			Outcome:  outcome,
			RuleID:   ru.id,
			RuleName: ru.name,
		}

		req := map[string]any{
			"database":   "siem",
			"collection": "securityevents",
			"operation":  "insert",
			"data":       e,
		}

		resp, err := db.Do(req)
		if err != nil {
			fmt.Printf("insert failed: err=%v\n", err)
			continue
		}
		if resp.Status != "success" {
			fmt.Printf("insert failed: status=%s msg=%s\n", resp.Status, resp.Message)
			continue
		}
		ok++
	}

	fmt.Printf("OK: inserted %d/%d test events into %s\n", ok, total, dbAddr)
}
