-- 1. История тренировок пользователя за период.
-- До оптимизации:
EXPLAIN ANALYZE
SELECT w.id, w.name, w.workout_date
FROM workouts w
WHERE w.user_id = '00000000-0000-0000-0000-000000000001'
  AND w.workout_date BETWEEN DATE '2026-01-01' AND DATE '2026-01-31'
ORDER BY w.workout_date DESC;

-- После индекса idx_workouts_user_date запрос использует
-- отбор по user_id и диапазон по workout_date без полного сканирования.
EXPLAIN ANALYZE
SELECT w.id, w.name, w.workout_date
FROM workouts w
WHERE w.user_id = '00000000-0000-0000-0000-000000000001'
  AND w.workout_date BETWEEN DATE '2026-01-01' AND DATE '2026-01-31'
ORDER BY w.workout_date DESC;


-- 2. Сводная статистика тренировок пользователя.
-- Переписанный запрос считает агрегаты одной выборкой.
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
