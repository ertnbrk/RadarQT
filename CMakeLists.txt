cmake_minimum_required(VERSION 3.16)

project(TelemetrySystem VERSION 0.1 LANGUAGES CXX)

# Add both applications as subdirectories
add_subdirectory(TelemetrySender)
add_subdirectory(TelemetryReceiver)