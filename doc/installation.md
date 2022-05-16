# Installation {#installation}

## <a name="linux">Linux</a>

Several packages are required to build CalcMySky.

On Ubuntu 20.04 they can be installed by the following command:
```
sudo apt install git cmake g++ libeigen3-dev libglm-dev libqt5opengl5-dev
```
On Fedora 35 the command is
```
sudo dnf install cmake gcc-c++ eigen3-devel glm-devel qt5-qttools-devel
```

To fetch and build CalcMySky, in a terminal issue the following commands:
```
git clone https://github.com/10110111/CalcMySky
cd CalcMySky
mkdir build
cd build
cmake ..
make
```

To install (by default into `/usr/local`), issue
```
sudo make install
```

## <a name="windows">Windows</a>

<span style="background-color: orange;">WARNING</span>: these instructions haven't been tested. The actual Windows builds were done via AppVeyor, using `.appveyor.yml` script in the root of the `CalcMySky` source tree.

The following instructions assume that the required packages are installed as follows:

 * Visual Studio 16 2019 is used
 * [CMake](https://cmake.org/) in such a way that its `cmake.exe` executable is accesible via the `%PATH%` environment variable
 * [Qt5](https://download.qt.io/archive/qt/) (version 5.9 is used in Stellarium) in `C:/Qt/5.9/msvc2017_64`
 * [GLM](https://github.com/g-truc/glm) in `C:/glm`
 * [Eigen3](https://eigen.tuxfamily.org) in `C:/eigen` (there should exist `C:/eigen/share/eigen3/cmake` directory)

In the terminal `cd` into the directory with `CalcMySky` unarchived, and run the following commands:
```
mkdir build install
cd build
cmake -G "Visual Studio 16 2019" -DADD_CXX_FLAGS="/IC:/glm" -DCMAKE_PREFIX_PATH="C:/Qt/5.9/msvc2017_64/lib/cmake/Qt5;C:/eigen/share/eigen3/cmake" -DCMAKE_INSTALL_PREFIX=../install ../CalcMySky
cmake --build . --config Release
cmake --build . --config Release --target install
cp -rv ../CalcMySky/examples ../install/CalcMySky
```

The output of these commands is the `install/CalcMySky` directory in the source tree.
