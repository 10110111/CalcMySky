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

To fetch and build CalcMySky, in a terminal issue the following commands, changing the number in `-DQT_VERSION=5` option to 6 if you're using Qt6:
```
git clone https://github.com/10110111/CalcMySky
cd CalcMySky
mkdir build
cd build
cmake .. -DQT_VERSION=5
make
```

To install (by default into `/usr/local`), issue
```
sudo make install
```

## <a name="windows">Windows</a>

<span style="background-color: orange;">WARNING</span>: these instructions haven't been tested. The actual Windows builds were done via AppVeyor, using `.appveyor.yml` script in the root of the `CalcMySky` source tree.

The following instructions assume that the required packages are installed as follows:

 * Visual Studio 16 2019 is used;
 * [CMake](https://cmake.org/) in such a way that its `cmake.exe` executable is accesible via the `%%PATH%` environment variable;
 * [Qt5 or Qt6](https://download.qt.io/archive/qt/) in the directory described by `%%QT_BASEDIR%` environment variable (there should exist `%%QT_BASEDIR%\bin` directory);
 * [GLM](https://github.com/g-truc/glm) in `C:/glm`;
 * [Eigen3](https://eigen.tuxfamily.org) in `C:/eigen` (there should exist `C:/eigen/share/eigen3/cmake` directory).

In the terminal `cd` into the directory with `CalcMySky` unarchived, and run the following commands, changing the number in `-DQT_VERSION=6` option to 5 if you're using Qt5:
```
mkdir build install
cd build
set PATH=%PATH%;%QT_BASEDIR%\bin
cmake -G "Visual Studio 16 2019" -DCMAKE_CXX_FLAGS="/IC:/glm" -DCMAKE_PREFIX_PATH="C:/eigen/share/eigen3/cmake" -DCMAKE_INSTALL_PREFIX=../install ../CalcMySky -DQT_VERSION=6
cmake --build . --config Release
cmake --build . --config Release --target install
cp -rv ../CalcMySky/examples ../install/CalcMySky
```

The output of these commands is the `install/CalcMySky` directory in the source tree.

## <a name="macos">macOS</a>

<span style="background-color: orange;">WARNING</span>: these instructions haven't been tested. The actual Windows builds were done via AppVeyor, using `.appveyor.yml` script in the root of the `CalcMySky` source tree.

The following instructions assume that the required packages are installed as follows:

 * [CMake](https://cmake.org/) in such a way that its `cmake` executable is accesible via the `$PATH` environment variable
 * [Qt5 or Qt6](https://download.qt.io/archive/qt/) in `$qtPath` (there should exist `$qtPath/bin` directory) — available in HomeBrew
 * [GLM](https://github.com/g-truc/glm) in `$glmPath` (there should exist `$glmPath/include` directory) — available in HomeBrew
 * [Eigen3](https://eigen.tuxfamily.org) in `$eigenPath` (there should exist `$eigenPath/share/eigen3/cmake` directory) — available in HomeBrew

In the terminal `cd` into the directory with `CalcMySky` unarchived, and run the following commands, changing the number in `-DQT_VERSION=6` option to 5 if you're using Qt5:
```
cd ~/projects/ && mkdir build && cd build
export PATH=$qtPath/bin:$PATH
cmake ../calcmysky -DCMAKE_BUILD_TYPE=Release \
                   -DCMAKE_INSTALL_PREFIX="$PWD/install/CalcMySky" \
                   -DQT_VERSION=6 \
                   -DCMAKE_PREFIX_PATH="$eigenPath/share/eigen3/cmake" \
                   -DCMAKE_CXX_FLAGS="-I$glmPath/include"
cmake --build .
cmake --build . --target install
```

The output of these commands is the `install/CalcMySky` directory in the source tree.
