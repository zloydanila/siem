cd ~/Рабочий\ стол/SIEM-agent

# 1. Запуск
docker compose down -v
docker compose up -d
docker compose ps

# 2. Заполнение данных

cd ~/Рабочий\ стол/siem/siem/siemcore
# 1. Build
go build -o gen_events ./cmd/gen_events
# 2. Запуск (отправляет события на DB Server)
CPPDB_ADDR="127.0.0.1:8080" ./gen_events
# Или с параметрами
./gen_events --count 100 --interval 1s

# 3. Открыть браузер
# http://localhost:8090
# admin / admin123
