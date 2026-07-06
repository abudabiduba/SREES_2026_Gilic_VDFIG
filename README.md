# Doubly-Fed Induction Generator (DFIG) Wind Turbine dTwin Plugin

This repository contains the dynamic DAE model converter plugin for the **dTwin** simulation platform, developed as part of the SREES 2026 project. The plugin parses IEEE power system XML test cases and generates optimized `.dmodl` (digital model) and `.vmodl` (visual model) files integrating Federico Milano's dq-frame DFIG wind turbine model equations.

---

## Features
- **Cross-Platform**: Compiles and runs out-of-the-box on both **Windows** (Visual Studio/MSVC) and **Linux** (GCC/Clang).
- **Self-Contained**: Includes fallbacks for required natID SDK headers (like `arch/MemoryOut.h`) to compile smoothly without manual SDK modifications.
- **Optimized Serialization**: Utilizes clean, lightweight buffer streams resulting in small output files (~2.2 KB instead of several MBs) and preventing memory corruption in the host application.
- **Imbalance & Crash Resolved**: Declares algebraic outputs (`P_e`, `P_h`, `Q_h`) as `[out=true]` parameters to ensure perfect variable-to-equation balance and prevent null-pointer dereferences in the solver's symbol table.

---

## Prerequisites
1. **CMake** (v3.18 or higher).
2. **C++20 Compiler**: Visual Studio 2019+ (Windows) or GCC 10+ / Clang 11+ (Linux).
3. **natID SDK**: Must be installed in your home directory:
   - **Windows**: `C:\Users\<YourUsername>\natID.SDK` (resolves via `%USERPROFILE%`)
   - **Linux**: `/home/<YourUsername>/natID.SDK` (resolves via `$HOME`)

---

## Download
Clone this repository to your local machine:
```bash
git clone https://github.com/abudabiduba/SREES_2026_Gilic_VDFIG.git
cd SREES_2026_Gilic_VDFIG
```

---

## Compilation

### Linux
To configure and compile the plugin in **Release** mode:
```bash
cd src
mkdir build_release && cd build_release
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```
This builds:
- The plugin library: `libdfigPlugin.so`
- The standalone test runner: `test_runner`

---

### Windows
Open **Developer PowerShell for VS** (or Command Prompt) and run:
```cmd
cd src
mkdir build
cd build
cmake ..
cmake --build . --config Release
```
This builds:
- The plugin library: `Release\dfigPlugin.dll`
- The standalone test runner: `Release\test_runner.exe`

---

## Installation in dTwin

To make the converter available in the `dTwin` graphical interface, copy the compiled library into your `dTwin` installation's `plugins/` directory:

- **Windows**: Copy the compiled `dfigPlugin.dll` (or use the pre-compiled version inside the `src/` folder of this repo) and paste it into:
  `C:\Program Files\dTwin\plugins\` (or your custom dTwin plugins directory).
- **Linux**: Copy the compiled `libdfigPlugin.so` (or the pre-compiled `src/libdfigPlugin.so`) and paste it into:
  `~/ba.natID/dTwin/plugins/`

After copying the binary, **restart `dTwin`**.

---

## How to Use

### Using the dTwin GUI
1. Launch **`dTwin`**.
2. Go to the top menu and select **Import -> Vjetroelektrana DFIG Converter**.
3. In the dialog:
   - **Input XML**: Choose one of the IEEE test case XMLs from the `src/testData/` directory (`case9.xml`, `case30.xml`, `case118.xml`, or `case300.xml`).
   - **Output DMODL**: Choose where you want to save the output model file (e.g., `test9.dmodl`).
4. Click **Convert**.
5. Close the plugin window, open the generated `.dmodl` file in `dTwin`, and click **Solve** to run the simulation.

### Using the Standalone Test Runner (Command Line)
You can test the conversion pipeline without running the GUI:
- **Linux**:
  ```bash
  ./build_release/test_runner /path/to/SREES_2026_Gilic_VDFIG/src
  ```
- **Windows**:
  ```cmd
  build\Release\test_runner.exe ..
  ```
This reads all XMLs in `src/testData/` and outputs compiled `.dmodl` and `.vmodl` files under `src/build/testOutput/`.
