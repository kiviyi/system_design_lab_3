FROM ghcr.io/userver-framework/ubuntu-22.04-userver:latest AS build

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j

FROM ghcr.io/userver-framework/ubuntu-22.04-userver:latest

WORKDIR /app
COPY --from=build /app/build/fitness-tracker-userver /app/fitness-tracker-userver
COPY configs /app/configs
COPY openapi.yaml /app/openapi.yaml
COPY db /app/db

EXPOSE 8080

CMD ["/app/fitness-tracker-userver", "--config", "/app/configs/static_config.yaml", "--config_vars", "/app/configs/config_vars.docker.yaml"]
