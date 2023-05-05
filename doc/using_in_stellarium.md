# Using in Stellarium {#using-in-stellarium}

Stellarium supports ShowMySky atmosphere model since Stellarium v1.0. This model is available for machines capable of OpenGL 3.3.

Having installed a ShowMySky-enabled version of Stellarium, launch it, open the _View_ dialog (accessible via <kbd>F4</kbd> key), and click _Atmosphere settings_ button (next to _Atmosphere visualization_ checkbox). _Atmosphere Details_ dialog will open, where in the _Choose model_ combobox you can select either _Preetham_, which is the legacy atmosphere model, or _ShowMySky_, which is powered by this library.

Having chosen _ShowMySky_ atmosphere, select a path to the atmosphere model data directory in the _Path to data_ field. The model data directory must contain `params.atmo` file. If it doesn't, it's not a model directory, or some files are missing. The path field marks the path in red if there's no `params.atmo` file in this directory, or the directory is not accessible.

If loading completes successfully, a text "Loaded successfully" will appear, otherwise a red-colored error message will be displayed.

## Eclipse simulation quality

There are four quality levels for simulation of eclipsed atmosphere:

1. This is the most basic, "fail-safe", level, not aimed at any realism — just darkens the sky depending on visibility of the Sun.
2. This level actually activates simulation of first-order scattering of eclipsed atmosphere, and use of a precomputed texture of second order, which is then mixed with non-eclipsed multiple-order scattering to approximate the view of partial eclipses.
3. At this level second-order scattering is simulated in real time, which takes quite a bit of GPU power and needs a fast GPU to achieve acceptable frame rate.
4. At this level second-order scattering is simulated as at level 3, and in addition, first-order is calculated individually for each pixel on the screen. This makes scenes with e.g. eclipse in twilight better-resolved, at the cost of additional loss of frame rate.

## Troubleshooting

Some common errors that can happen during loading of the ShowMySky model are discussed below.

### Failed to load ShowMySky library: ... No such file or directory

This may be caused by `ShowMySky` library not having been installed (it comes with the `CalcMySky` package), or not being in the library search paths.

On GNU/Linux systems `libShowMySky.so` should usually be under `/usr/lib` or `/usr/local/lib` directory tree (e.g. `/usr/lib/x86_64-linux-gnu/libShowMySky.so`, or in a non-standard directory added to `LD_LIBRARY_PATH` environment variable or listed in `/etc/ld.so.conf`.

On Windows systems the library should either be in the same directory as `Stellarium.exe`, or the directory containing `ShowMySky.dll` should be listed in the `PATH` environment variable.

### ABI version of ShowMySky library is 14, but this program has been compiled against version 13.

If the `ShowMySky` library has larger value of the ABI version, then Stellarium needs to be recompiled to use it. Otherwise you may need to upgrade the library. A special case is when the ABI version is larger than 500000000. This corresponds to an older versioning scheme. In this case the highest digit should be omitted when comparing versions.
