Dependencies for CI builds
==========================

This directory hosts parts of dependencies which are needed for Windows version of arrow1 to build on Travis CI. These files are not available for install with Chocolatey package manager and obtaining them from their original sources during the build is problematic. For these reasons we put C/C++ header files and Visual C++-compatible import libraries in the repo, which is generally not great but, not terrible. C API of libsndfile and libjack is stable, so arrow1 built this way will be binary compatible with prebuilt releases:

- [libsndfile](http://www.mega-nerd.com/libsndfile/#Download) - tested with version 1.0.28
- [Jack](https://jackaudio.org/downloads/) - tested with version 1.9.11

We build only 64bit version of arrow1 on Windows. The import libraries we store here don't include any executable code of the dependencies, they only tell the compiler that some symbols may be found in .dll files, whose installation is the responsibility of the user.
