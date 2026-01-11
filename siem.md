cd ~/Рабочий\ стол/SIEM-agent

# 1. Запуск
docker compose down -v
docker compose up -d
docker compose ps

# 2. Заполнение данных

cd siem/siemcore/cmd/gen_events
ionin@ionin-MCLF-XX:~/Рабочий стол/siem/siem/siemcore/cmd/gen_events$ DB_ADDR=centerbeam.proxy.rlwy.net:35453 go run main.go

# 3. Открыть браузер
# http://localhost:8090
# admin / admin123
