## Installation

### Linux

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

### Windows

<span style="background-color: red;">TODO</span>: write this section

<!-- The following packages are required to build CalcMySky.

 * [Git](https://git-scm.com/)
 * [CMake](https://cmake.org/)
 * [Qt5](https://download.qt.io/archive/qt/)
 * [GLM](https://github.com/g-truc/glm)
 * [Eigen3](https://eigen.tuxfamily.org)
 * A C++ compiler, e.g. GCC or Clang -->
