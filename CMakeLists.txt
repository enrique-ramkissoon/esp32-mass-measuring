cmake_minimum_required(VERSION 3.13)

project(esp32MassMeasure)

add_executable(massMeasure src/main.c)

# Tell IDF build to link against this target.
set(IDF_PROJECT_EXECUTABLE massMeasure)

# Add FreeRTOS as a subdirectory. AFR_BOARD tells which board to target.
set(AFR_BOARD espressif.esp32_devkitc CACHE INTERNAL "")
add_subdirectory(amazon-freertos)

# Link against the mqtt library so that we can use it. Dependencies are transitively
# linked.
target_link_libraries(massMeasure PRIVATE AFR::mqtt)
