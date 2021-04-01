# ESP32 Diagnostics System (Embedded System Code)

This is one of two repositories which make up the ESP32-based Diagnostic System for ECNG3020
This repository contains the code which runs on the ESP32.

## Setup

These setup instructions are intended for use on Linux.

### Step 1 - Install Build Tools
- Follow the Section "Setting up the Toolchain" in the Amazon FreeRTOS Documentation : https://docs.aws.amazon.com/freertos/latest/userguide/getting_started_espressif.html


### Step 1 - Download Amazon FreeRTOS submodule
- Navigate to the repository folder in a terminal.
- Run the command `git submodule update --init --recursive`

### Step 2 - Configure and Compile Project
- Run the included "run_cmake.sh" script to generate the build files
- Navigate to the build folder
    - `cd build`
- Enable FreeRTOS to collect run time statistics
    - Run the command `make menuconfig`
    - Enable Component Config > FreeRTOS > Enable FreeRTOS to collect run time stats
- Compile the project
    - `make all`

### Step 3 - Flash the Program to an ESP32
- Ensure that the ESP32 is connected to a USB port
- Run the command `make flash monitor`
