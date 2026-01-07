package app

import (
	"crypto/subtle"
	"net/http"
)

func (a *App) auth(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		u, p, ok := r.BasicAuth()
		if !ok {
			needAuth(w, `Basic realm="siem", charset="UTF-8"`)
			return
		}

		if subtle.ConstantTimeCompare([]byte(u), []byte(a.cfg.User)) != 1 ||
			subtle.ConstantTimeCompare([]byte(p), []byte(a.cfg.Pass)) != 1 {
			needAuth(w, `Basic realm="siem", charset="UTF-8"`)
			return
		}

		next(w, r)
	}
}

func needAuth(w http.ResponseWriter, hdr string) {
	w.Header().Set("WWW-Authenticate", hdr)
	http.Error(w, "unauthorized", http.StatusUnauthorized)
}
