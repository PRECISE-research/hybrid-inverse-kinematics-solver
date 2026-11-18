# hybrid-inverse-kinematics-solver

[![Build and Test](https://github.com/PRECISE-research/hybrid-inverse-kinematics-solver/actions/workflows/continuous_integration.yml/badge.svg)](https://github.com/PRECISE-research/hybrid-inverse-kinematics-solver/actions/workflows/continuous_integration.yml)

A command-line tool that runs an OpenSim `InverseKinematicsSolver` over any
combination of **markers**, **IMU/segment orientations**, and **prescribed
coordinates** in a single solve, driven by one XML setup file.

The stock OpenSim IK tool tracks markers *or* orientations. This one tracks
both at once, and lets you additionally prescribe coordinates from a `.mot`
file — useful when part of the kinematic chain is measured directly (a pelvis
from body kinematics, say) and the rest has to be solved from IMUs.

## Quick start

You do not need to build this, and you do not need OpenSim installed.

1. Download `hybrid-inverse-kinematics-solver-X.Y.Z-windows-x64.zip` from the
   [latest release](https://github.com/PRECISE-research/hybrid-inverse-kinematics-solver/releases/latest).
2. Unzip it anywhere.
3. Point it at a setup file:

```bash
hybrid-inverse-kinematics-solver.exe -S path\to\ikSetup.xml
```

The zip carries the OpenSim and SimTK libraries next to the executable, so it
runs on a machine with no OpenSim installed and is unaffected by what is on
`PATH`. Results are written beside the setup file.

`examples/` in this repository holds three ready-to-run setups with their data —
see [Examples](#examples). Writing your own is covered under
[Setup file](#setup-file).

Build from source only if you are changing the tool.

## Building from source

### Requirements

| Dependency | Version tested | Notes |
|---|---|---|
| OpenSim | 4.4 | `C:/OpenSim 4.4` |
| RapidXML | 1.13 | vendored in `external/rapidxml/`; nothing to install |
| CMake | 3.21+ | 3.31.8 used here; 3.21 is the hard floor |
| Compiler | MSVC 19.44 (VS 2022) | C++17 |

Windows is the tested platform. Linux and macOS should build from source
against an OpenSim built for them, but are untested.

### Build

OpenSim cannot be found automatically, so its path is passed explicitly
(RapidXML is vendored in the repository and found on its own). Adjust the
path to match your machine:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOpenSim_DIR="C:/OpenSim 4.4/cmake" -DCMAKE_INSTALL_PREFIX="./install"
```

```bash
cmake --build build --config Release
```

`CMAKE_INSTALL_PREFIX` is set because the default on Windows is
`C:/Program Files (x86)/...`, which needs administrator rights. Installing
gives a self-contained copy, with the OpenSim and SimTK DLLs bundled next to
the executable, so the install tree runs on a machine with no OpenSim:

```bash
cmake --install build --config Release
```

#### If the built exe exits immediately after printing its banner

It loaded the wrong OpenSim DLLs: on a machine with more than one OpenSim
installed, Windows takes whichever comes first on `PATH`, and a binary linked
against 4.4 dies as soon as it touches 4.5's DLLs. The build copies the right
DLLs next to the exe so this should not happen, but a build tree configured
before that step existed lacks them. Rebuild, or put the matching `bin` first
on `PATH`:

```bash
PATH="/c/OpenSim 4.4/bin:$PATH" ./build/src/Release/hybrid-inverse-kinematics-solver.exe -S examples/imu-orientations/ikSetup.xml
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
  <constraintWeight>Infinity</constraintWeight>  <!-- the default; see below -->
  <solverAccuracy>1e-05</solverAccuracy>
</inverseKinematicsSetup>
```

`<rotation>` is a space-fixed XYZ sequence in degrees, matching OpenSim's own
OpenSense setup files.

`<constraintWeight>` is best left at `Infinity`, which is also what you get
without the element. Any finite value turns the model's constraints and locked
coordinates into weighted goals rather than enforcing them, and the solver can
then stop after a step or two on a frame. You won't get an error, just frames
that freeze or jump and coordinates parked at their range limits. The tool
prints a warning when the weight is finite and the model has constraints or
locked coordinates.

Numbers in the setup file are read strictly. `Inf` and `Infinity` are accepted
for any weight, and a value with trailing text such as `10abc` is an error (or
a skipped weight) rather than being read as `10`.

Any element the parser does not recognise is reported as a warning rather than
ignored, so typos and obsolete tags (`<applyRotation>`, superseded by
`<rotation>`) show up instead of quietly doing nothing.

Names that do not exist in the model are reported as warnings and skipped, not
treated as errors. Alongside `resultsOutputPath` the tool writes
`marker_errors.sto` and `orientation_errors.sto` into the same directory.

## Tests

```bash
ctest --test-dir build -C Release
```

Thirteen tests, about 10 seconds. They put the OpenSim the binary was built against
first on `PATH` themselves, so they are immune to the multi-install problem above.

**Error paths** (9, sub-second each) drive the setup-file parser with malformed
input: unparseable XML, no root element, a missing `modelFilePath`, nothing to
track, a weight with no `name`, a non-numeric weight, a non-numeric
`constraintWeight`, an unrecognised element, and a setup file that does not
exist. Each asserts the exit code and the
message. Every one of these was a crash, an uninitialised read, or silence
before the guards existed.

**Golden-file regression** (4) solves each example and compares the resulting
`.sto` against a committed reference, column by column. The tolerance is 1e-6
where results reproduce exactly on any machine, and 1-5 degrees where weakly
constrained joints vary between CPUs (see `tests/CMakeLists.txt`).
The three examples cover the three tracking paths; `tests/cases/rotation_90.xml`
adds a fourth because every shipped example uses `<rotation>0. 0. 0.</rotation>`,
which makes the degrees-to-radians conversion a no-op and would leave that path
untested.

The references were produced with **OpenSim 4.4** and are sensitive to it. After
an upgrade, small differences are expected and absorbed by the tolerance; larger
ones deserve investigation. Once you have read the diff and understand it:

```bash
cmake --build build --config Release --target regenerate-test-references
```

Do not reach for that to turn a red test green without reading what it was
reporting first — the failure message names the column and row that moved.

Configure with `-DBUILD_TESTING=OFF` to skip the tests, e.g. without Python.

## Continuous integration

Every push runs `.github/workflows/continuous_integration.yml` on a Windows
runner: configure, Release build, all 13 tests, and a bundling install that
proves the executable's DLL set is complete. OpenSim 4.5.2 is downloaded once,
pruned to ~80 MB, and cached -- later runs restore it in seconds. The golden
references were produced with OpenSim 4.4. CI's results match them exactly for
imu-orientations and to within the tolerances in `tests/CMakeLists.txt` for the
others, because weakly constrained joints settle slightly differently on
different CPUs.

### Cutting a release

1. Bump `project(VERSION X.Y.Z)` in `CMakeLists.txt` -- the tool's banner
   prints this, and the release workflow refuses a tag that disagrees with it.
2. Add the release to [CHANGELOG.md](CHANGELOG.md), and set `version` and
   `date-released` in `CITATION.cff`.
3. `git tag X.Y.Z && git push origin X.Y.Z`

The tag triggers `.github/workflows/github_release.yml`, which rebuilds, reruns
the full test suite, and attaches
`hybrid-inverse-kinematics-solver-X.Y.Z-windows-x64.zip` to a GitHub release.
The zip is self-contained -- unzip anywhere and run the exe, no OpenSim
installation needed. A tag like `X.Y.Z-rc1` publishes as a prerelease.

## Related work

This tool is a thin layer over OpenSim's `InverseKinematicsSolver`; the
modelling and the solver itself are OpenSim's work. If you publish results
produced with it, cite OpenSim as well as this repository:

- Delp SL, Anderson FC, Arnold AS, et al. (2007). OpenSim: open-source software
  to create and analyze dynamic simulations of movement. *IEEE Transactions on
  Biomedical Engineering*, 54(11), 1940-1950.
  [doi:10.1109/TBME.2007.901024](https://doi.org/10.1109/TBME.2007.901024)
- Seth A, Hicks JL, Uchida TK, et al. (2018). OpenSim: Simulating musculoskeletal
  dynamics and neuromuscular control to study human and animal movement. *PLOS
  Computational Biology*, 14(7), e1006223.
  [doi:10.1371/journal.pcbi.1006223](https://doi.org/10.1371/journal.pcbi.1006223)
- Al Borno M, O'Day J, Ibarra V, et al. (2022). OpenSense: An open-source toolbox
  for inertial-measurement-unit-based measurement of lower extremity kinematics
  over long durations. *Journal of NeuroEngineering and Rehabilitation*, 19, 22.
  [doi:10.1186/s12984-022-01001-x](https://doi.org/10.1186/s12984-022-01001-x)

XML parsing uses [RapidXML](http://rapidxml.sourceforge.net/) 1.13, vendored
under `external/rapidxml/` (Boost Software Licence / MIT).

## License

MIT -- see [LICENSE](LICENSE). Third-party material is carved out in
[NOTICE](NOTICE): RapidXML is vendored under `external/rapidxml/` under its own
terms, the release archives bundle OpenSim and Simbody/SimTK under Apache-2.0,
and the example models are adapted from MIT-licensed OpenSim models, whose
notices and citations are listed there. The carve-out is kept out of LICENSE deliberately -- any text
appended there stops GitHub and Zenodo identifying the licence at all.

Cite this tool via [CITATION.cff](CITATION.cff) (GitHub renders a "Cite this
repository" button from it), and cite OpenSim alongside it -- see
[Related work](#related-work).

## Examples

Run any of them with `-S`; each writes `ik_results.sto` beside its setup file.

| Example | Data | Exercises |
|---|---|---|
| `examples/imu-orientations` | 4 IMU orientations, 6.7 s | orientations + prescribed pelvis coordinates |
| `examples/markerless-theia` | Theia key points + segment rotations, 1 s | markers **and** orientations together |
| `examples/markers-cycling` | 74 optical markers, one pedal revolution | markers only, with weights spanning 0 to 1000 |

```bash
./build/src/Release/hybrid-inverse-kinematics-solver.exe -S examples/imu-orientations/ikSetup.xml
```

The example data is deidentified, and trimmed to what each solve actually needs.
The cycling trial is cut to 800 frames (4 s at 200 Hz, one revolution at
15 rpm) from an original 57,266. In the Theia example, each IMU frame in the
model carries a fixed rotation onto the Theia segment axes; the setup file
explains how it was estimated.

`Geometry/` folders are **not** included — the `.vtp`/`.stl` meshes are display
only and add ~130 MB per example. OpenSim logs a `Couldn't find file` warning
for each missing mesh and solves normally. Point the models at your own
geometry directory if you want to turn `useVisualizer` on.
