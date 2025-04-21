# Compiling Xenon with Visual Studio 2022

### (Prerequisite) Download [**Git for Windows**](https://git-scm.com/download/win)

Go through the Git for Windows installation as normal

### (Prerequisite) Download the Community edition from [**Visual Studio 2022**](https://visualstudio.microsoft.com/vs/)

Once you are within the installer:

1. Select `Desktop development with C++`
2. Go to "Individual Components" tab
3. Search and select `C++ Clang Compiler for Windows` if you want to compile with Clang and `MSBuild support for LLVM` (Obligatory)
4. Continue the installation

### Cloning the source code

1. Open Git for Windows, navigate to a place where you want to store the Xenon source code folder
2. Clone the repository by running `git clone --recursive https://github.com/xenon-emu/Xenon.git`

### Compiling with Visual Studio GUI

1. Open up Visual Studio, select `Open a local folder` and select the folder with the Xenon source code. The folder should contain `CMakeLists.txt`
2. You can choose between `x64-MSVC-Release` or `x64-Clang-Release`
3. Compile

Your Xenon.exe will be in `C:\path\to\source\Build\x64-Clang-Release\`