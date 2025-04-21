# Compiling Xenon with Visual Studio 2022 and WSL CMake Deploy
### Now, this is by no means the best way to do it, it is certainly *a* way
### Prerequisites
* WSL2 with Ubuntu 24.04 or higher
* WSL2 configured to have X11 support
* Visual Studio 2022 with CMake support, and Linux support
* A lot of sanity and luck
### Setup
#### First, you need to satisfy dependencies for Ubuntu, which follows the same steps above
Then you need to satisfy MSBuild which Microsoft has a build dependency list for ['Targeting Linux using VS2022 and WSL2'](https://learn.microsoft.com/en-us/cpp/build/walkthrough-build-debug-wsl2?view=msvc-170)
### This should pretty much be it, just open the project as a local folder and set the target
If it prompts you to load the CMake files, **please** do so otherwise this **will NOT** work.
### How to run
#### By default, it will copy to ~/.vs/xenon/build/${target}
You will need to modify xenon_config.toml to point to the proper paths. I recommend copying your Xbox folder to your home folder, but if you don't want to, then you can just change `C:` to `/mnt/c`