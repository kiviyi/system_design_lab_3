CREATE TABLE IF NOT EXISTS users (
    id UUID PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    first_name VARCHAR(100) NOT NULL,
    last_name VARCHAR(100) NOT NULL,
    email VARCHAR(255) NOT NULL UNIQUE,
    hashed_password VARCHAR(255) NOT NULL,
    CONSTRAINT users_email_format_chk CHECK (position('@' in email) > 1)
);

CREATE TABLE IF NOT EXISTS exercises (
    id UUID PRIMARY KEY,
    name VARCHAR(200) NOT NULL,
    description VARCHAR(1000) NOT NULL DEFAULT '',
    muscle_group VARCHAR(100) NOT NULL
);

CREATE TABLE IF NOT EXISTS workouts (
    id UUID PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name VARCHAR(200) NOT NULL,
    workout_date DATE NOT NULL
);

CREATE TABLE IF NOT EXISTS workout_exercises (
    id BIGSERIAL PRIMARY KEY,
    workout_id UUID NOT NULL REFERENCES workouts(id) ON DELETE CASCADE,
    exercise_id UUID NOT NULL REFERENCES exercises(id),
    sets INTEGER NOT NULL,
    reps INTEGER NOT NULL,
    weight NUMERIC(8, 2) NOT NULL DEFAULT 0,
    CONSTRAINT workout_exercises_sets_chk CHECK (sets BETWEEN 1 AND 100),
    CONSTRAINT workout_exercises_reps_chk CHECK (reps BETWEEN 1 AND 1000),
    CONSTRAINT workout_exercises_weight_chk CHECK (weight >= 0)
);
