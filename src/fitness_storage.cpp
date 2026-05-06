#include "fitness_storage.hpp"

#include <algorithm>
#include <utility>

#include <userver/formats/common/type.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/query.hpp>
#include <userver/utils/uuid4.hpp>

#include "api_utils.hpp"

USERVER_NAMESPACE_BEGIN

namespace fitness_tracker {

namespace {

constexpr auto kMaster = storages::postgres::ClusterHostType::kMaster;

std::optional<std::string> ToSqlLikePattern(const std::optional<std::string>& pattern) {
  if (!pattern) return std::nullopt;
  auto value = *pattern;
  std::replace(value.begin(), value.end(), '*', '%');
  return value;
}

const std::string kUserJsonSql = R"(
SELECT json_build_object(
    'id', u.id::text,
    'username', u.username,
    'first_name', u.first_name,
    'last_name', u.last_name,
    'email', u.email
)::text
FROM users u
WHERE u.id = $1
)";

const std::string kWorkoutJsonByIdSql = R"(
SELECT json_build_object(
    'id', w.id::text,
    'user_id', w.user_id::text,
    'name', w.name,
    'date', to_char(w.workout_date, 'YYYY-MM-DD'),
    'exercises', COALESCE((
        SELECT json_agg(
            json_build_object(
                'exercise_id', we.exercise_id::text,
                'exercise_name', e.name,
                'sets', we.sets,
                'reps', we.reps,
                'weight', we.weight
            )
            ORDER BY we.id
        )
        FROM workout_exercises we
        JOIN exercises e ON e.id = we.exercise_id
        WHERE we.workout_id = w.id
    ), '[]'::json)
)::text
FROM workouts w
WHERE w.id = $1
)";

}  // namespace

FitnessStorage::FitnessStorage(const components::ComponentConfig& config, const components::ComponentContext& context)
    : LoggableComponentBase(config, context),
      pg_cluster_(context.FindComponent<components::Postgres>("postgres-db").GetCluster()) {}

User FitnessStorage::CreateUser(
    const std::string& username,
    const std::string& first_name,
    const std::string& last_name,
    const std::string& email,
    const std::string& password
) {
  ValidateUser(username, first_name, last_name, email, password);

  const auto existing_user = pg_cluster_->Execute(
      kMaster,
      "SELECT username, email FROM users WHERE username = $1 OR email = $2",
      username,
      email
  );
  if (!existing_user.IsEmpty()) {
    const auto row = existing_user[0];
    if (row["username"].As<std::string>() == username) {
      throw ApiException(server::http::HttpStatus::kConflict, "Username already exists");
    }
    throw ApiException(server::http::HttpStatus::kConflict, "Email already exists");
  }

  const auto user_id = utils::generators::GenerateUuid();
  const auto created = QuerySingleJson(
      kMaster,
      R"(
      INSERT INTO users(id, username, first_name, last_name, email, hashed_password)
      VALUES($1::uuid, $2, $3, $4, $5, $6)
      RETURNING json_build_object(
          'id', id::text,
          'username', username,
          'first_name', first_name,
          'last_name', last_name,
          'email', email
      )::text
      )",
      user_id,
      username,
      first_name,
      last_name,
      email,
      HashPassword(password)
  );

  return ParseUserJson(created);
}

std::string FitnessStorage::Login(const std::string& username, const std::string& password) {
  const auto result = pg_cluster_->Execute(
      kMaster,
      "SELECT username, hashed_password FROM users WHERE username = $1",
      username
  );
  if (result.IsEmpty()) {
    throw ApiException(server::http::HttpStatus::kUnauthorized, "Incorrect username or password");
  }

  const auto row = result[0];
  if (row["hashed_password"].As<std::string>() != HashPassword(password)) {
    throw ApiException(server::http::HttpStatus::kUnauthorized, "Incorrect username or password");
  }

  const auto token = utils::generators::GenerateUuid();
  active_tokens_[token] = row["username"].As<std::string>();
  return token;
}

std::string FitnessStorage::Authorize(const server::http::HttpRequest& request) const {
  const auto header = request.GetHeader("Authorization");
  if (!header.starts_with(kBearerPrefix)) {
    throw ApiException(server::http::HttpStatus::kUnauthorized, "Could not validate credentials");
  }

  const auto token = std::string{header.substr(kBearerPrefix.size())};
  const auto token_it = active_tokens_.find(token);
  if (token_it == active_tokens_.end()) {
    throw ApiException(server::http::HttpStatus::kUnauthorized, "Could not validate credentials");
  }
  return token_it->second;
}

std::vector<User> FitnessStorage::SearchUsers(
    const std::optional<std::string>& username,
    const std::optional<std::string>& first_name,
    const std::optional<std::string>& last_name
) const {
  if (!username && !first_name && !last_name) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Provide at least one search parameter");
  }

  std::vector<User> users;
  if (username) {
    const auto result = pg_cluster_->Execute(
        kMaster,
        "SELECT id::text, username, first_name, last_name, email FROM users WHERE username = $1 ORDER BY username",
        *username
    );
    for (const auto& row : result) {
      users.push_back(User{
          .id = row["id"].As<std::string>(),
          .username = row["username"].As<std::string>(),
          .first_name = row["first_name"].As<std::string>(),
          .last_name = row["last_name"].As<std::string>(),
          .email = row["email"].As<std::string>(),
          .hashed_password = {},
      });
    }
    return users;
  }

  const auto sql_first = ToSqlLikePattern(first_name);
  const auto sql_last = ToSqlLikePattern(last_name);

  const auto result = [&]() {
    if (sql_first && sql_last) {
      return pg_cluster_->Execute(
          kMaster,
          "SELECT id::text, username, first_name, last_name, email FROM users WHERE lower(first_name) LIKE lower($1) AND lower(last_name) LIKE lower($2) ORDER BY username",
          *sql_first,
          *sql_last
      );
    }
    if (sql_first) {
      return pg_cluster_->Execute(
          kMaster,
          "SELECT id::text, username, first_name, last_name, email FROM users WHERE lower(first_name) LIKE lower($1) ORDER BY username",
          *sql_first
      );
    }
    return pg_cluster_->Execute(
        kMaster,
        "SELECT id::text, username, first_name, last_name, email FROM users WHERE lower(last_name) LIKE lower($1) ORDER BY username",
        *sql_last
    );
  }();

  for (const auto& row : result) {
    users.push_back(User{
        .id = row["id"].As<std::string>(),
        .username = row["username"].As<std::string>(),
        .first_name = row["first_name"].As<std::string>(),
        .last_name = row["last_name"].As<std::string>(),
        .email = row["email"].As<std::string>(),
        .hashed_password = {},
    });
  }
  return users;
}

Exercise FitnessStorage::CreateExercise(const std::string& name, const std::string& description, const std::string& muscle_group) {
  if (name.empty() || name.size() > 200) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Exercise name must be between 1 and 200 characters");
  }
  if (description.size() > 1000) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Exercise description cannot exceed 1000 characters");
  }
  if (muscle_group.empty() || muscle_group.size() > 100) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Muscle group must be between 1 and 100 characters");
  }

  const auto exercise_id = utils::generators::GenerateUuid();
  const auto created = QuerySingleJson(
      kMaster,
      R"(
      INSERT INTO exercises(id, name, description, muscle_group)
      VALUES($1::uuid, $2, $3, $4)
      RETURNING json_build_object(
          'id', id::text,
          'name', name,
          'description', description,
          'muscle_group', muscle_group
      )::text
      )",
      exercise_id,
      name,
      description,
      muscle_group
  );
  return ParseExerciseJson(created);
}

std::vector<Exercise> FitnessStorage::GetExercises() const {
  std::vector<Exercise> exercises;
  const auto result = pg_cluster_->Execute(
      kMaster,
      "SELECT id::text, name, description, muscle_group FROM exercises ORDER BY name"
  );
  for (const auto& row : result) {
    exercises.push_back(Exercise{
        .id = row["id"].As<std::string>(),
        .name = row["name"].As<std::string>(),
        .description = row["description"].As<std::string>(),
        .muscle_group = row["muscle_group"].As<std::string>(),
    });
  }
  return exercises;
}

Workout FitnessStorage::CreateWorkout(
    const std::string& user_id,
    const std::string& name,
    const std::optional<std::string>& date
) {
  if (name.empty() || name.size() > 200) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Workout name must be between 1 and 200 characters");
  }
  if (date && !IsValidDate(*date)) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Date must be in YYYY-MM-DD format");
  }

  EnsureUserExists(user_id);

  const auto workout_id = utils::generators::GenerateUuid();
  const auto created = QuerySingleJson(
      kMaster,
      R"(
      INSERT INTO workouts(id, user_id, name, workout_date)
      VALUES($1::uuid, $2::uuid, $3, $4::date)
      RETURNING json_build_object(
          'id', id::text,
          'user_id', user_id::text,
          'name', name,
          'date', to_char(workout_date, 'YYYY-MM-DD'),
          'exercises', '[]'::json
      )::text
      )",
      workout_id,
      user_id,
      name,
      date.value_or(CurrentDateIso())
  );
  return ParseWorkoutJson(created);
}

Workout FitnessStorage::AddExerciseToWorkout(
    const std::string& user_id,
    const std::string& workout_id,
    const std::string& exercise_id,
    int sets,
    int reps,
    double weight
) {
  if (sets < 1 || sets > 100) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Sets must be between 1 and 100");
  }
  if (reps < 1 || reps > 1000) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Reps must be between 1 and 1000");
  }
  if (weight < 0) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Weight cannot be negative");
  }

  EnsureUserExists(user_id);

  const auto workout_check = pg_cluster_->Execute(
      kMaster,
      "SELECT user_id::text FROM workouts WHERE id = $1::uuid",
      workout_id
  );
  if (workout_check.IsEmpty()) {
    throw ApiException(server::http::HttpStatus::kNotFound, "Workout not found");
  }
  if (workout_check[0]["user_id"].As<std::string>() != user_id) {
    throw ApiException(server::http::HttpStatus::kForbidden, "Workout does not belong to this user");
  }

  const auto exercise_check = pg_cluster_->Execute(
      kMaster,
      "SELECT 1 FROM exercises WHERE id = $1::uuid",
      exercise_id
  );
  if (exercise_check.IsEmpty()) {
    throw ApiException(server::http::HttpStatus::kNotFound, "Exercise not found");
  }

  pg_cluster_->Execute(
      kMaster,
      R"(
      INSERT INTO workout_exercises(workout_id, exercise_id, sets, reps, weight)
      VALUES($1::uuid, $2::uuid, $3, $4, $5)
      )",
      workout_id,
      exercise_id,
      sets,
      reps,
      weight
  );

  return ParseWorkoutJson(QuerySingleJson(kMaster, kWorkoutJsonByIdSql, workout_id));
}

std::vector<Workout> FitnessStorage::GetUserWorkouts(const std::string& user_id) const {
  EnsureUserExists(user_id);

  const auto json = QueryArrayJson(
      kMaster,
      R"(
      SELECT COALESCE(json_agg(workout_json ORDER BY workout_date DESC), '[]'::json)::text
      FROM (
          SELECT
              w.workout_date,
              json_build_object(
                  'id', w.id::text,
                  'user_id', w.user_id::text,
                  'name', w.name,
                  'date', to_char(w.workout_date, 'YYYY-MM-DD'),
                  'exercises', COALESCE((
                      SELECT json_agg(
                          json_build_object(
                              'exercise_id', we.exercise_id::text,
                              'exercise_name', e.name,
                              'sets', we.sets,
                              'reps', we.reps,
                              'weight', we.weight
                          )
                          ORDER BY we.id
                      )
                      FROM workout_exercises we
                      JOIN exercises e ON e.id = we.exercise_id
                      WHERE we.workout_id = w.id
                  ), '[]'::json)
              ) AS workout_json
          FROM workouts w
          WHERE w.user_id = $1::uuid
      ) t
      )",
      user_id
  );

  std::vector<Workout> workouts;
  for (const auto& item : json) {
    workouts.push_back(ParseWorkoutJson(item));
  }
  return workouts;
}

formats::json::Value FitnessStorage::GetWorkoutStats(
    const std::string& user_id,
    const std::string& start_date,
    const std::string& end_date
) const {
  if (!IsValidDate(start_date) || !IsValidDate(end_date)) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Date must be in YYYY-MM-DD format");
  }

  EnsureUserExists(user_id);

  const auto json = QueryArrayJson(
      kMaster,
      R"(
      SELECT COALESCE(json_agg(workout_json ORDER BY workout_date ASC, workout_name ASC), '[]'::json)::text
      FROM (
          SELECT
              w.workout_date,
              w.name AS workout_name,
              json_build_object(
                  'id', w.id::text,
                  'user_id', w.user_id::text,
                  'name', w.name,
                  'date', to_char(w.workout_date, 'YYYY-MM-DD'),
                  'exercises', COALESCE((
                      SELECT json_agg(
                          json_build_object(
                              'exercise_id', we.exercise_id::text,
                              'exercise_name', e.name,
                              'sets', we.sets,
                              'reps', we.reps,
                              'weight', we.weight
                          )
                          ORDER BY we.id
                      )
                      FROM workout_exercises we
                      JOIN exercises e ON e.id = we.exercise_id
                      WHERE we.workout_id = w.id
                  ), '[]'::json)
              ) AS workout_json
          FROM workouts w
          WHERE w.user_id = $1::uuid
            AND w.workout_date BETWEEN $2::date AND $3::date
      ) t
      )",
      user_id,
      start_date,
      end_date
  );

  int total_exercises = 0;
  int total_sets = 0;
  int total_reps = 0;
  int total_workouts = 0;
  formats::json::ValueBuilder workouts_builder(formats::common::Type::kArray);

  for (const auto& item : json) {
    ++total_workouts;
    const auto workout = ParseWorkoutJson(item);
    workouts_builder.PushBack(WorkoutToJson(workout));
    total_exercises += static_cast<int>(workout.exercises.size());
    for (const auto& exercise : workout.exercises) {
      total_sets += exercise.sets;
      total_reps += exercise.sets * exercise.reps;
    }
  }

  formats::json::ValueBuilder builder(formats::common::Type::kObject);
  builder["user_id"] = user_id;
  builder["start_date"] = start_date;
  builder["end_date"] = end_date;
  builder["total_workouts"] = total_workouts;
  builder["total_exercises"] = total_exercises;
  builder["total_sets"] = total_sets;
  builder["total_reps"] = total_reps;
  builder["workouts"] = workouts_builder.ExtractValue();
  return builder.ExtractValue();
}

formats::json::Value FitnessStorage::UserToJson(const User& user) const {
  formats::json::ValueBuilder builder(formats::common::Type::kObject);
  builder["id"] = user.id;
  builder["username"] = user.username;
  builder["first_name"] = user.first_name;
  builder["last_name"] = user.last_name;
  builder["email"] = user.email;
  return builder.ExtractValue();
}

formats::json::Value FitnessStorage::ExerciseToJson(const Exercise& exercise) const {
  formats::json::ValueBuilder builder(formats::common::Type::kObject);
  builder["id"] = exercise.id;
  builder["name"] = exercise.name;
  builder["description"] = exercise.description;
  builder["muscle_group"] = exercise.muscle_group;
  return builder.ExtractValue();
}

formats::json::Value FitnessStorage::WorkoutToJson(const Workout& workout) const {
  formats::json::ValueBuilder builder(formats::common::Type::kObject);
  builder["id"] = workout.id;
  builder["user_id"] = workout.user_id;
  builder["name"] = workout.name;
  builder["date"] = workout.date;

  formats::json::ValueBuilder exercises_builder(formats::common::Type::kArray);
  for (const auto& exercise : workout.exercises) {
    formats::json::ValueBuilder exercise_builder(formats::common::Type::kObject);
    exercise_builder["exercise_id"] = exercise.exercise_id;
    exercise_builder["exercise_name"] = exercise.exercise_name;
    exercise_builder["sets"] = exercise.sets;
    exercise_builder["reps"] = exercise.reps;
    exercise_builder["weight"] = exercise.weight;
    exercises_builder.PushBack(exercise_builder.ExtractValue());
  }

  builder["exercises"] = exercises_builder.ExtractValue();
  return builder.ExtractValue();
}

void FitnessStorage::ValidateUser(
    const std::string& username,
    const std::string& first_name,
    const std::string& last_name,
    const std::string& email,
    const std::string& password
) const {
  if (username.size() < 3 || username.size() > 50) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Username must be between 3 and 50 characters");
  }
  if (first_name.empty() || first_name.size() > 100) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "First name must be between 1 and 100 characters");
  }
  if (last_name.empty() || last_name.size() > 100) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Last name must be between 1 and 100 characters");
  }
  if (!IsValidEmail(email)) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Invalid email");
  }
  if (password.size() < 6) {
    throw ApiException(server::http::HttpStatus::kBadRequest, "Password must be at least 6 characters");
  }
}

void FitnessStorage::EnsureUserExists(const std::string& user_id) const {
  const auto result = pg_cluster_->Execute(
      kMaster,
      "SELECT 1 FROM users WHERE id = $1::uuid",
      user_id
  );
  if (result.IsEmpty()) {
    throw ApiException(server::http::HttpStatus::kNotFound, "User not found");
  }
}

User FitnessStorage::ParseUserJson(const formats::json::Value& json) const {
  return User{
      .id = json["id"].As<std::string>(),
      .username = json["username"].As<std::string>(),
      .first_name = json["first_name"].As<std::string>(),
      .last_name = json["last_name"].As<std::string>(),
      .email = json["email"].As<std::string>(),
      .hashed_password = {},
  };
}

Exercise FitnessStorage::ParseExerciseJson(const formats::json::Value& json) const {
  return Exercise{
      .id = json["id"].As<std::string>(),
      .name = json["name"].As<std::string>(),
      .description = json["description"].As<std::string>(),
      .muscle_group = json["muscle_group"].As<std::string>(),
  };
}

Workout FitnessStorage::ParseWorkoutJson(const formats::json::Value& json) const {
  Workout workout{
      .id = json["id"].As<std::string>(),
      .user_id = json["user_id"].As<std::string>(),
      .name = json["name"].As<std::string>(),
      .date = json["date"].As<std::string>(),
      .exercises = {},
  };

  if (json.HasMember("exercises")) {
    for (const auto& item : json["exercises"]) {
      workout.exercises.push_back(WorkoutExercise{
          .exercise_id = item["exercise_id"].As<std::string>(),
          .exercise_name = item["exercise_name"].As<std::string>(),
          .sets = item["sets"].As<int>(),
          .reps = item["reps"].As<int>(),
          .weight = item["weight"].As<double>(),
      });
    }
  }

  return workout;
}

formats::json::Value FitnessStorage::QuerySingleJson(
    storages::postgres::ClusterHostType host_type,
    const std::string& query
) const {
  const auto result = pg_cluster_->Execute(host_type, query);
  if (result.IsEmpty()) {
    return {};
  }
  return formats::json::FromString(result[0][0].As<std::string>());
}

}  // namespace fitness_tracker

USERVER_NAMESPACE_END
