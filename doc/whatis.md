# What is CalcMySky {#whatis}

CalcMySky is a software package that simulates scattering of light by the atmosphere to render daytime and twilight skies (without stars). Its primary purpose is to enable realistic view of the sky in applications such as planetaria. Secondary objective is to make it possible to explore atmospheric effects such as glories, fogbows etc., as well as simulate unusual environments such as on Mars or an exoplanet orbiting a star with a non-solar spectrum of radiation.

The simulation is based on E. Bruneton's [Precomputed Atmospheric Scattering](https://hal.inria.fr/inria-00288758/en) paper and the [updated implementation of the demo](https://ebruneton.github.io/precomputed_atmospheric_scattering). This in particular limits the atmosphere to spherical symmetry (which means localized clouds are not supported, and ground albedo is the same all around the globe).

An additional capability is simulation of solar eclipses, which is currently limited to two [scattering orders](single-multiple-scattering.html#scattering-order), while the non-eclipsed atmosphere can be simulated to arbitrary order.

This package consists of three parts:

 * `calcmysky` utility that does the precomputation of the atmosphere model to enable rendering,
 * `libShowMySky` library that lets the applications render the atmosphere model,
 * `ShowMySky` preview GUI that makes it possible to preview the rendering of the atmosphere model and examine its properties.
