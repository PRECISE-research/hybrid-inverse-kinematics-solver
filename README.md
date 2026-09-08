# hybrid-inverse-kinematics-solver

A command-line tool that runs an OpenSim `InverseKinematicsSolver` over any
combination of **markers**, **IMU/segment orientations**, and **prescribed
coordinates** in a single solve, driven by one XML setup file.

The stock OpenSim IK tool tracks markers *or* orientations. This one tracks
both at once, and lets you additionally prescribe coordinates from a `.mot`
file — useful when part of the kinematic chain is measured directly (a pelvis
from body kinematics, say) and the rest has to be solved from IMUs.

## Requirements

| Dependency | Version tested | Notes |
|---|---|---|
| OpenSim | 4.4 | `C:/OpenSim 4.4` |
| RapidXML | 1.13 | header-only; `C:/dev/rapidxml-1.13` |
| CMake | 3.21+ | 3.31.8 used here; 3.21 is the hard floor |
| Compiler | MSVC 19.44 (VS 2022) | C++17 |

## Build

`CMakePresets.json` already holds the paths above:

```bash
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
```

If OpenSim or RapidXML live elsewhere, override them without editing the
presets:

```bash
cmake --preset windows-msvc -DOpenSim_DIR="D:/OpenSim 4.4/cmake" -DRapidXML_ROOT=D:/src/rapidxml-1.13
```

Install a self-contained copy (the OpenSim and SimTK DLLs are bundled next to
the executable, so the install tree runs on a machine with no OpenSim):

```bash
cmake --install build/windows-msvc --config Release
```

### If the built exe exits immediately after printing its banner

It found the wrong OpenSim DLLs. Windows resolves them by `PATH` order, so a
machine with more than one OpenSim installed will happily load 4.5's DLLs into
a binary linked against 4.4, and the process dies as soon as it touches one.
Either run the **installed** copy from `install/bin` (which carries its own
DLLs and is immune), or put the matching `bin` first:

```bash
PATH="/c/OpenSim 4.4/bin:$PATH" ./build/windows-msvc/Release/hybrid-inverse-kinematics-solver.exe -S examples/imu-orientations/ikSetup.xml
```

## Usage

```
Option            Argument          Action / Notes
------            --------          --------------
-Help, -H                           Print the command-line options.
-Setup, -S        SetupFileName     Specify an xml setup file.
```

Two environment variables help when a solve misbehaves:

- `IK_DEBUG_LOG` — turn on OpenSim debug-level logging.
- `IK_DIAG_ONLY` — solve frame 0 only, dump the assembler's constraint weight,
  error norm, tolerance and accuracy, then exit.

## Setup file

Paths inside the setup file are resolved **relative to the setup file itself**,
so an example folder can be moved anywhere. All three top-level data blocks are
optional; supply whichever combination the trial calls for.

```xml
<?xml version="1.0" encoding="utf-8"?>
<inverseKinematicsSetup version="1.0">
  <modelFilePath>./model.osim</modelFilePath>

  <markers>                                  <!-- optional -->
    <filePath>./markers.trc</filePath>
    <weights>
      <markerWeight name="RASI">1000</markerWeight>
    </weights>
  </markers>

  <orientations>                             <!-- optional -->
    <filePath>./orientations.sto</filePath>
    <weights>
      <orientationWeight name="femur_r_imu">1</orientationWeight>
    </weights>
    <rotation>0. 0. 0.</rotation>            <!-- space-fixed XYZ, degrees -->
    <applyRotation>false</applyRotation>
  </orientations>

  <coordinates>                              <!-- optional -->
    <filePath>./coordinates.mot</filePath>
    <weights>
      <orientationWeight name="pelvis_tilt">1</orientationWeight>
    </weights>
    <inDegrees>true</inDegrees>
  </coordinates>

  <resultsOutputPath>./ik_results.sto</resultsOutputPath>
  <useVisualizer>false</useVisualizer>
  <constraintWeight>10</constraintWeight>
  <solverAccuracy>1e-05</solverAccuracy>
</inverseKinematicsSetup>
```

Names that do not exist in the model are reported as warnings and skipped, not
treated as errors. Alongside `resultsOutputPath` the tool writes
`marker_errors.sto` and `orientation_errors.sto` into the same directory.

## Examples

Run any of them with `-S`; each writes `ik_results.sto` beside its setup file.

| Example | Data | Exercises |
|---|---|---|
| `examples/imu-orientations` | 4 IMU orientations, 6.7 s | orientations + prescribed pelvis coordinates |
| `examples/markerless-theia` | Theia key points + segment rotations, 1 s | markers **and** orientations together |
| `examples/markers-cycling` | 74 optical markers, one pedal revolution | markers only, with weights spanning 0 to 1000 |

```bash
./build/windows-msvc/Release/hybrid-inverse-kinematics-solver.exe -S examples/imu-orientations/ikSetup.xml
```

The example data is deidentified, and trimmed to what each solve actually needs.
The cycling trial is truncated to its first 800 frames (4 s at 200 Hz, one
revolution at 15 rpm) from an original 57,266.

`Geometry/` folders are **not** included — the `.vtp`/`.stl` meshes are display
only and add ~130 MB per example. OpenSim logs a `Couldn't find file` warning
for each missing mesh and solves normally. Point the models at your own
geometry directory if you want to turn `useVisualizer` on.
