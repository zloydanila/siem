package app

import (
	"net/http"

	"siemcore/internal/cppdb"
)

type Config struct {
	User     string
	Pass     string
	DBClient *cppdb.Client
}

type App struct {
	cfg Config
	mux *http.ServeMux
}

func New(cfg Config) http.Handler {
	a := &App{cfg: cfg, mux: http.NewServeMux()}

	a.mux.HandleFunc("/login", a.handleLoginPage)
	a.mux.HandleFunc("/dashboard", a.handleDashboardPage)
	a.mux.HandleFunc("/events", a.handleEventsPage)
	a.mux.HandleFunc("/logout", a.handleLogout)

	a.mux.HandleFunc("/api/health", a.handleHealth)
	a.mux.HandleFunc("/api/logout-trigger", a.handleLogoutTrigger)

	a.mux.HandleFunc("/api/dashboard", a.auth(a.handleAPIDashboard))
	a.mux.HandleFunc("/api/events", a.auth(a.handleAPIEvents))
	a.mux.HandleFunc("/api/events/", a.auth(a.handleAPIEventsByID))
	a.mux.HandleFunc("/api/events.json", a.auth(a.handleAPIEventsJSONExport))
	a.mux.HandleFunc("/api/events.csv", a.auth(a.handleAPIEventsCSV))

	fs := http.FileServer(http.Dir("web/static"))
	a.mux.Handle("/static/", http.StripPrefix("/static/", fs))

	a.mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		http.Redirect(w, r, "/login", http.StatusFound)
	})

	return a.mux
}
