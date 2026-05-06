#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <userver/components/loggable_component_base.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "api_types.hpp"

USERVER_NAMESPACE_BEGIN

namespace fitness_tracker {

class FitnessStorage final : public components::LoggableComponentBase {
 public:
  static constexpr std::string_view kName = "fitness-storage";

  FitnessStorage(const components::ComponentConfig& config, const components::ComponentContext& context);

  User CreateUser(
      const std::string& username,
      const std::string& first_name,
      const std::string& last_name,
      const std::string& email,
      const std::string& password
  );

  std::string Login(const std::string& username, const std::string& password);
  std::string Authorize(const server::http::HttpRequest& request) const;

  std::vector<User> SearchUsers(
      const std::optional<std::string>& username,
      const std::optional<std::string>& first_name,
      const std::optional<std::string>& last_name
  ) const;

  Exercise CreateExercise(const std::string& name, const std::string& description, const std::string& muscle_group);
  std::vector<Exercise> GetExercises() const;

  Workout CreateWorkout(const std::string& user_id, const std::string& name, const std::optional<std::string>& date);
  Workout AddExerciseToWorkout(
      const std::string& user_id,
      const std::string& workout_id,
      const std::string& exercise_id,
      int sets,
      int reps,
      double weight
  );
  std::vector<Workout> GetUserWorkouts(const std::string& user_id) const;
  formats::json::Value GetWorkoutStats(
      const std::string& user_id,
      const std::string& start_date,
      const std::string& end_date
  ) const;

  formats::json::Value UserToJson(const User& user) const;
  formats::json::Value ExerciseToJson(const Exercise& exercise) const;
  formats::json::Value WorkoutToJson(const Workout& workout) const;

 private:
  void ValidateUser(
      const std::string& username,
      const std::string& first_name,
      const std::string& last_name,
      const std::string& email,
      const std::string& password
  ) const;
  void EnsureUserExists(const std::string& user_id) const;
  User ParseUserJson(const formats::json::Value& json) const;
  Exercise ParseExerciseJson(const formats::json::Value& json) const;
  Workout ParseWorkoutJson(const formats::json::Value& json) const;
  formats::json::Value QuerySingleJson(
      storages::postgres::ClusterHostType host_type,
      const std::string& query
  ) const;

  template <typename... Args>
  formats::json::Value QuerySingleJson(
      storages::postgres::ClusterHostType host_type,
      const std::string& query,
      const Args&... args
  ) const;

  template <typename... Args>
  formats::json::Value QueryArrayJson(
      storages::postgres::ClusterHostType host_type,
      const std::string& query,
      const Args&... args
  ) const;

  storages::postgres::ClusterPtr pg_cluster_;
  std::unordered_map<std::string, std::string> active_tokens_;
};

template <typename... Args>
formats::json::Value FitnessStorage::QuerySingleJson(
    storages::postgres::ClusterHostType host_type,
    const std::string& query,
    const Args&... args
) const {
  const auto result = pg_cluster_->Execute(host_type, query, args...);
  if (result.IsEmpty()) {
    return {};
  }
  return formats::json::FromString(result[0][0].template As<std::string>());
}

template <typename... Args>
formats::json::Value FitnessStorage::QueryArrayJson(
    storages::postgres::ClusterHostType host_type,
    const std::string& query,
    const Args&... args
) const {
  const auto result = pg_cluster_->Execute(host_type, query, args...);
  if (result.IsEmpty()) {
    return formats::json::FromString("[]");
  }
  return formats::json::FromString(result[0][0].template As<std::string>());
}

}  // namespace fitness_tracker

USERVER_NAMESPACE_END
