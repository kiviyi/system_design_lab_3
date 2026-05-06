# Fitness Tracker API on userver + PostgreSQL

REST API для фитнес-трекера, переписанный на `userver` по образцу проекта из `homework_2`, но уже с выполнением требований домашнего задания 03: проектирование PostgreSQL-схемы, индексов, SQL-оптимизации и подключение API к реляционной базе данных.

## Что сделано

- проект переведен на `C++20` и `userver`
- API сохраняет ту же предметную модель: пользователи, упражнения, тренировки
- вместо in-memory хранения используется PostgreSQL
- в репозиторий добавлены SQL-артефакты задания:
  - схема БД
  - тестовые данные
  - индексы
  - запросы для `EXPLAIN ANALYZE`
  - пояснительный отчет

## Стек

- C++20
- userver
- PostgreSQL 16
- CMake
- Docker Compose

## Структура проекта

```text
homework_3/
├── CMakeLists.txt
├── Dockerfile
├── README.md
├── configs/
│   ├── config_vars.docker.yaml
│   ├── config_vars.yaml
│   └── static_config.yaml
├── db/
│   ├── indexes.sql
│   ├── optimization.sql
│   ├── report.md
│   ├── schema.sql
│   └── seed.sql
├── docker-compose.yaml
├── openapi.yaml
└── src/
    ├── api_types.hpp
    ├── api_utils.cpp
    ├── api_utils.hpp
    ├── fitness_storage.cpp
    ├── fitness_storage.hpp
    ├── handlers.cpp
    ├── handlers.hpp
    └── main.cpp
```

## Сборка

Нужен установленный `userver` с PostgreSQL-компонентом.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Запуск

### Локально

Нужен запущенный PostgreSQL и схема из каталога `db/`.

```bash
./build/fitness-tracker-userver \
  --config configs/static_config.yaml \
  --config_vars configs/config_vars.yaml
```

Сервис стартует на `http://localhost:8080`.

### Docker

```bash
docker-compose up --build
```

После запуска:

- API: `http://localhost:8080`
- OpenAPI: `openapi.yaml`

Контейнер PostgreSQL доступен только внутри docker-сети проекта, чтобы не конфликтовать с локальной БД на хосте.

## API

| Метод | Endpoint | Описание |
|-------|----------|----------|
| POST | `/register` | Регистрация пользователя |
| POST | `/token` | Получение bearer-токена |
| POST | `/users` | Создание пользователя |
| GET | `/users/search` | Поиск по логину или маске имени/фамилии |
| POST | `/exercises` | Создание упражнения, нужен `Authorization: Bearer <token>` |
| GET | `/exercises` | Список упражнений |
| POST | `/users/{user_id}/workouts` | Создание тренировки |
| POST | `/users/{user_id}/workouts/{workout_id}/exercises` | Добавление упражнения в тренировку |
| GET | `/users/{user_id}/workouts` | История тренировок |
| GET | `/users/{user_id}/workouts/stats` | Статистика за период |

## SQL-артефакты

- [db/schema.sql](/Users/voros_ykq/Documents/system_design/homework_3/db/schema.sql) — схема БД
- [db/seed.sql](/Users/voros_ykq/Documents/system_design/homework_3/db/seed.sql) — тестовые данные
- [db/indexes.sql](/Users/voros_ykq/Documents/system_design/homework_3/db/indexes.sql) — индексы
- [db/optimization.sql](/Users/voros_ykq/Documents/system_design/homework_3/db/optimization.sql) — запросы для анализа производительности
- [db/report.md](/Users/voros_ykq/Documents/system_design/homework_3/db/report.md) — описание сущностей, ключей, ограничений и оптимизации

## Примеры

Регистрация:

```bash
curl -X POST http://localhost:8080/register \
  -H "Content-Type: application/json" \
  -d '{
    "username": "john",
    "first_name": "John",
    "last_name": "Doe",
    "email": "john@example.com",
    "password": "secret123"
  }'
```

Логин:

```bash
curl -X POST http://localhost:8080/token \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "username=john&password=secret123"
```

Создание упражнения:

```bash
curl -X POST http://localhost:8080/exercises \
  -H "Authorization: Bearer <TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Жим лежа",
    "description": "Базовое упражнение для грудных мышц",
    "muscle_group": "Грудь"
  }'
```

## Важно

Локально я не смог прогнать финальную сборку `userver`, потому что в этой среде нет установленного SDK `userver` с PostgreSQL-компонентом. Поэтому я честно перевел проект на правильный стек и структуру, но финальную компиляцию нужно проверять уже в окружении, где `userver` доступен.
