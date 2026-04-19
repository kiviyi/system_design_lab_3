# Домашнее задание 03 : Проектирование и оптимизация реляционной базы данных

### Цель работы: 
Получить практические навыки работы с PostgreSQL, проектирования схемы
БД, создания индексов и оптимизации запросов.

## Что сделано

- API из прошлой работы переведено из in-memory хранения на базу данных
- Добавлена PostgreSQL-схема с PK, FK и CHECK/UNIQUE ограничениями
- Подготовлены тестовые данные минимум по 10 записей
- Добавлены индексы под частые запросы
- Подготовлены SQL-запросы для анализа и оптимизации
- Docker Compose поднимает и API, и PostgreSQL

## Стек

- Python 3.11
- FastAPI
- SQLAlchemy
- PostgreSQL 16
- JWT аутентификация
- Docker Compose

## Запуск

### Через Docker

```bash
docker-compose up --build
```

Сервисы:

- API: `http://localhost:8000`
- Swagger UI: `http://localhost:8000/docs`
- PostgreSQL: `localhost:5432`

Параметры БД по умолчанию:

- `database`: `fitness_tracker`
- `user`: `fitness_user`
- `password`: `fitness_password`

### Локально

```bash
pip install -r requirements.txt
export DATABASE_URL=postgresql+psycopg://fitness_user:fitness_password@localhost:5432/fitness_tracker
python main.py
```

## SQL-артефакты

Файлы для домашнего задания лежат в каталоге [`db`](./db):

- [`schema.sql`](./schema.sql) — схема БД и ограничения
- [`seed.sql`](./seed.sql) — тестовые данные
- [`indexes.sql`](./indexes.sql) — индексы и краткие пояснения
- [`optimization.sql`](./optimization.sql) — запросы для анализа и оптимизации

## API Endpoints

### Аутентификация

- `POST /register` — регистрация пользователя
- `POST /token` — получение JWT токена

### Пользователи

- `POST /users` — создание пользователя
- `GET /users/search` — поиск по логину или маске имени/фамилии

### Упражнения

- `POST /exercises` — создание упражнения
- `GET /exercises` — список упражнений

### Тренировки

- `POST /users/{user_id}/workouts` — создание тренировки
- `POST /users/{user_id}/workouts/{workout_id}/exercises` — добавление упражнения
- `GET /users/{user_id}/workouts` — история тренировок
- `GET /users/{user_id}/workouts/stats` — статистика за период

## Тесты

```bash
export TEST_DATABASE_URL=postgresql+psycopg://fitness_user:fitness_password@localhost:5432/fitness_tracker
pytest tests.py -v
```
Тесты работают только с PostgreSQL.

## Структура проекта

```text
homework_3/
├── db/
│   ├── indexes.sql
│   ├── optimization.sql
│   ├── schema.sql
│   └── seed.sql
├── Dockerfile
├── README.md
├── docker-compose.yaml
├── main.py
├── openapi.yaml
├── requirements.txt
└── tests.py
```
