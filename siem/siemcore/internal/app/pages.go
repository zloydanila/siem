package app

import (
	"crypto/subtle"
	"net/http"
	"os"
	"time"
)

func (a *App) handleLoginPage(w http.ResponseWriter, r *http.Request) {
	b, err := os.ReadFile("web/templates/login.html")
	if err != nil {
		http.Error(w, "template error", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.WriteHeader(http.StatusOK)
	w.Write(b)
}

func (a *App) handleDashboardPage(w http.ResponseWriter, r *http.Request) {
	b, err := os.ReadFile("web/templates/dashboard.html")
	if err != nil {
		http.Error(w, "template error", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.WriteHeader(http.StatusOK)
	w.Write(b)
}

func (a *App) handleEventsPage(w http.ResponseWriter, r *http.Request) {
	b, err := os.ReadFile("web/templates/events.html")
	if err != nil {
		http.Error(w, "template error", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.WriteHeader(http.StatusOK)
	w.Write(b)
}

func (a *App) handleLogout(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
	w.Header().Set("Pragma", "no-cache")
	w.Header().Set("Expires", "0")
	
	http.Redirect(w, r, "/login", http.StatusSeeOther)
}

func (a *App) handleLogoutTrigger(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Cache-Control", "no-store, must-revalidate, max-age=0")
	realm := `Basic realm="logout-trigger-` + time.Now().UTC().Format("20060102T150405Z") + `"`
	w.Header().Set("WWW-Authenticate", realm)
	http.Error(w, "logout trigger", http.StatusUnauthorized)
}

func (a *App) handleHealth(w http.ResponseWriter, r *http.Request) {
	u, p, ok := r.BasicAuth()
	if !ok {
		needAuth(w, `Basic realm="health"`)
		return
	}

	if subtle.ConstantTimeCompare([]byte(u), []byte(a.cfg.User)) != 1 ||
		subtle.ConstantTimeCompare([]byte(p), []byte(a.cfg.Pass)) != 1 {
		needAuth(w, `Basic realm="health"`)
		return
	}

	writeJSON(w, http.StatusOK, map[string]any{"status": "ok"})
}
