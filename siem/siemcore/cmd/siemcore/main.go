package main

import (
	"crypto/tls"
	"log"
	"net/http"
	"os"
	"time"

	"siemcore/internal/app"
	"siemcore/internal/cppdb"
)

func main() {
	// ✅ ИЗМЕНЕНИЕ: default = 0.0.0.0:8088 (доступно с хоста)
	httpAddr := getenv("SIEM_HTTP_ADDR", "0.0.0.0:8088")
	dbAddr := getenv("CPPDB_ADDR", "127.0.0.1:8080")

	user := os.Getenv("SIEM_USER")
	pass := os.Getenv("SIEM_PASS")
	if user == "" || pass == "" {
		log.Fatal("SIEM_USER/SIEM_PASS must be set")
	}

	certFile := os.Getenv("SIEM_TLS_CERT")
	keyFile := os.Getenv("SIEM_TLS_KEY")

	dbClient := &cppdb.Client{
		Addr:    dbAddr,
		Timeout: 5 * time.Second,
	}

	h := app.New(app.Config{
		User:     user,
		Pass:     pass,
		DBClient: dbClient,
	})

	srv := &http.Server{
		Addr:         httpAddr,
		Handler:      h,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 15 * time.Second,
		IdleTimeout:  60 * time.Second,
		TLSConfig: &tls.Config{
			MinVersion: tls.VersionTLS12,
		},
	}

	log.Printf("Starting SIEM server on %s", httpAddr)
	
	if certFile != "" && keyFile != "" {
		log.Fatal(srv.ListenAndServeTLS(certFile, keyFile))
	}
	log.Fatal(srv.ListenAndServe())
}

func getenv(k, def string) string {
	v := os.Getenv(k)
	if v == "" {
		return def
	}
	return v
}
