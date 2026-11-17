# Changelog

## 1.0.1 (2026-11-17)

### Fixed
- The examples set `<constraintWeight>10</constraintWeight>`. A finite weight makes the model's constraints soft goals, and the solver could stop early on a frame: frames froze or jumped and coordinates parked at their range limits, with no error. The examples now use the default, Infinity, and a finite weight prints a warning when the model has constraints or locked coordinates.
- Numbers in the setup file are read strictly. `Infinity` was read as 0 and `10abc` as 10.
- Relative paths in the setup file now resolve when it is given as a bare file name (`-S ikSetup.xml`).
- The built exe exited silently after its banner when another OpenSim version came first on `PATH`. The build now copies the OpenSim DLLs next to the exe.
- markerless-theia: the IMU frames in the model were about 90 degrees off the Theia segment axes, so the orientations fought the markers. Also dropped the `worldbody` point, which sat at the origin throughout.
- markers-cycling: the example was the first 4 s of the trial, before pedalling started. It is now one crank revolution from later in the same trial.

### Changed
- Solves take longer at the default constraint weight than at weight 10, which was fast mainly because it stopped early.
- Two golden tests allow for small differences between machines (2 and 5 degrees).

## 1.0.0 (2026-09-08)

First release.
