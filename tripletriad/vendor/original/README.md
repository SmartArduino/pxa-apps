# TTcards

A remake of SquareSoft's Final Fantasy VIII Triple Triad card game in C++. The underlying code base spawned another project, [nomlib](https://github.com/i8degrees/nomlib/), while developing the game.

No machine reverse engineering has been done here, other than using ripped audio samples & sprite images; see CREDITS.md for details). This clone is derived in its entirety by many hours of my own study and speculation of how the game logic might be implemented.

## Dependencies

First, ensure that you have the following development packages setup:

* [cmake v3.0+](http://www.cmake.org/)
* [git](http://git-scm.com/download/)
* [nomlib](https://github.com/i8degrees/nomlib/)

### Mac OS X

**NOTE:** Requires clang with the libc++ standard library (v3.x through v5.x are known to work for me on OS X).

**NOTE:** My own build setup for TTcards can be found under the project's ```bin``` directory.

```shell
git clone "https://github.com/i8degrees/ttcards" ttcards.git
cd ttcards.git
mkdir build && cd build
#-DCMAKE_OSX_DEPLOYMENT_TARGET=10.11 is the minimum MacOS SDK version supported!
cmake -DDEBUG=on -DDEBUG_ASSERT=on -DNOMLIB_DEPS_PREFIX=$HOME/Library/Frameworks -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11 ..
make
make install
```

**<library_deps_path>** should be replaced with an absolute file path of
nomlib's third-party directory for OS X, i.e.: ```~/Projects/nomlib.git/third-party/osx```.

Upon a successful build, you should have an OS X application bundle under your current build directory called ```ttcards.app```.

### Linux

Linux builds are broken at the moment.

~~Until I get around to writing the proper instructions, you may take a look at my
.travis.yml build script in the project root of ttcards for hints!~~

### Microsoft Windows Vista, 7, 8

**NOTE:** Vista support has not been tested.

**NOTE:** My own build setup for TTcards can be found at ```bin/windev_tt.bat```

Requires [Microsoft Visual Studio Express 2013 for Windows](http://www.microsoft.com/visualstudio/eng#downloads).

  - Required software tools in your system PATH
    * MSVSCPP -- ```<MSVSCPP_INSTALL_PATH>\bin```
    * CMake -- ```<CMAKE_INSTALL_PATH>\bin```

```shell
git clone https://github.com/i8degrees/ttcards
cd ttcards
mkdir build && cd build
cmake -G"Visual Studio 12" -DARCH_32=on -DDEBUG=on -DDEBUG_ASSERT=on -DNOMLIB_DEPS_PREFIX=<library_deps_path> ..
```
**<library_deps_path>** should be replaced with an absolute file path of
nomlib's third-party directory for Windows, i.e.: ```%HOMEPATH%\Projects\nomlib.git\third-party\windows```.

Upon a successful generation, you should have populated your current build directory with a Visual Studio solution file called ```ttcards.vcxproj``` that you can then open and build from.

### Project Options ###

Build options are passed to cmake with the -D option. For example, to change the installation prefix:

```shell
cmake -D CMAKE_INSTALL_PREFIX=$HOME/Applications ..
```

  - Installation path: -D CMAKE_INSTALL_PREFIX=**\<DIRECTORY_PREFIX\>**
    * Defaults to your current **build** directory

  - Debugging: -D DEBUG=**\<BOOLEAN\>** -D DEBUG_ASSERT=**\<BOOLEAN\>**
    * Defaults to **OFF**

  - Universal binary: -D UNIVERSAL=**\<BOOLEAN\>**
    * Defaults to **OFF**

Removal is provided by executing **make uninstall** within your current build directory.

## Why ##

I was interested in getting back into game programming, and thought this would be an excellent long-term exercise in re-acquainting myself with C++, object-oriented design and general video game technology.

## Other Remakes ##

- Other remakes I am aware of
  * [Triple Triad Advance](http://www.ttadvance.ca/index.php/)
  * [Triple Triad Flash Online](http://www.tripletriadflashonline.com/)
  * [Android Triple Triad](https://github.com/Viish/Android-Triple-Triad/)
  * [The 7 Elders](http://www.kongregate.com/games/TadeuBAS/the-7-elders/)
