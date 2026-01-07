cat > siem/siemcore/run.sh << 'EOF'
#!/bin/bash

set -e

SIEM_DIR=${SIEM_DIR:-.}

echo "[run.sh] Starting SIEM Core..."
echo "  HTTP Server: ${SIEM_HTTP_ADDR:-0.0.0.0:8088}"
echo "  DB Address:  ${CPPDB_ADDR:-127.0.0.1:8080}"
echo "  User:        ${SIEM_USER:-admin}"

$SIEM_DIR/siemcore
EOF

chmod +x siem/siemcore/run.sh
