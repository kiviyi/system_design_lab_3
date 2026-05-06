-- Для проверки "до оптимизации" выполняйте запросы на схеме без индексов из indexes.sql.
-- Для проверки "после оптимизации" повторно запускайте те же запросы после создания индексов.

-- 1. История тренировок пользователя за период.
EXPLAIN ANALYZE
SELECT w.id, w.name, w.workout_date
FROM workouts w
WHERE w.user_id = '00000000-0000-0000-0000-000000000001'
  AND w.workout_date BETWEEN DATE '2026-01-01' AND DATE '2026-01-31'
ORDER BY w.workout_date DESC;

-- 2. Сводная статистика тренировок пользователя.
-- Оптимизация достигается за счет индекса по (user_id, workout_date)
-- и индекса по workout_id для таблицы workout_exercises.
EXPLAIN ANALYZE
SELECT
    w.user_id,
    COUNT(DISTINCT w.id) AS total_workouts,
    COUNT(we.id) AS total_exercises,
    COALESCE(SUM(we.sets), 0) AS total_sets,
    COALESCE(SUM(we.sets * we.reps), 0) AS total_reps
FROM workouts w
LEFT JOIN workout_exercises we ON we.workout_id = w.id
WHERE w.user_id = '00000000-0000-0000-0000-000000000001'
  AND w.workout_date BETWEEN DATE '2026-01-01' AND DATE '2026-01-31'
GROUP BY w.user_id;


-- 3. Детализация упражнений внутри тренировки.
-- Здесь помогает индекс idx_workout_exercises_workout_id.
EXPLAIN ANALYZE
SELECT
    we.workout_id,
    e.name,
    we.sets,
    we.reps,
    we.weight
FROM workout_exercises we
JOIN exercises e ON e.id = we.exercise_id
WHERE we.workout_id = '20000000-0000-0000-0000-000000000001';
