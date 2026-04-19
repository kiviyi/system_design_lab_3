CREATE INDEX IF NOT EXISTS idx_workouts_user_id ON workouts(user_id);
CREATE INDEX IF NOT EXISTS idx_workouts_user_date ON workouts(user_id, workout_date DESC);
CREATE INDEX IF NOT EXISTS idx_workout_exercises_workout_id ON workout_exercises(workout_id);
CREATE INDEX IF NOT EXISTS idx_workout_exercises_exercise_id ON workout_exercises(exercise_id);
CREATE INDEX IF NOT EXISTS idx_exercises_muscle_group ON exercises(muscle_group);

COMMENT ON INDEX idx_workouts_user_id IS
'Ускоряет поиск всех тренировок конкретного пользователя.';

COMMENT ON INDEX idx_workouts_user_date IS
'Ускоряет историю тренировок и статистику за период по user_id + workout_date.';

COMMENT ON INDEX idx_workout_exercises_workout_id IS
'Ускоряет JOIN тренировок с упражнениями внутри тренировки.';

COMMENT ON INDEX idx_workout_exercises_exercise_id IS
'Ускоряет аналитику и фильтрацию по конкретному упражнению.';

COMMENT ON INDEX idx_exercises_muscle_group IS
'Ускоряет выборки упражнений по группе мышц.';
