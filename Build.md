# VirtualDecks Build & Packaging Guide

This document provides complete instructions for building VirtualDecks (`juceDjApp`) on Windows and Linux, and creating distributable packages for each platform.

## Table of Contents

- [Prerequisites](#prerequisites)
  - [Windows](#windows)
  - [Linux](#linux)
- [Building from Source](#building-from-source)
  - [Windows](#windows-build)
  - [Linux](#linux-build)
- [Creating Distributable Packages](#creating-distributable-packages)
  - [Windows Packages](#windows-packages)
    - [Portable ZIP](#portable-zip)
    - [NSIS Installer](#nsis-installer)
  - [Linux Packages](#linux-packages)
    - [AppImage](#appimage)
    - [DEB Package](#deb-package)
    - [RPM Package](#rpm-package)

---

## Prerequisites

> **Note:** JUCE, TagLib, and xwax are downloaded automatically during the CMake configuration phase via `FetchContent`. You do **not** need to manually download them.

### Windows

1. **Visual Studio 2022** or **Visual Studio Build Tools 2022**
   - Required for C++ compilation with MSVC.
   - [Download](https://visualstudio.microsoft.com/downloads/)

2. **CMake** (version 3.25+)
   - [Download](https://cmake.org/download/) or install via `winget install cmake`

3. **Git**
   - Required by CMake to fetch dependencies (JUCE, TagLib, xwax).
   - [Download](https://git-scm.com/downloads) or `winget install --id Git.Git -e --source winget`

4. **Ninja** (recommended for faster builds)
   - [Download](https://github.com/ninja-build/ninja/releases) or `winget install ninja`
   - Extract and add to `PATH`

5. **NSIS** (for creating .exe installer)
   - [Download](http://nsis.sourceforge.net/Main_Page) or `winget install nsis`

### Linux

#### Ubuntu / Debian

```bash
sudo apt update && sudo apt upgrade -y

# Build tools & Git (Git is required for FetchContent)
sudo apt install -y build-essential cmake ninja-build git pkg-config

# Graphics and UI libraries
sudo apt install -y libx11-dev libxext-dev libxrandr-dev libfreetype6-dev libgl1-mesa-dev

# Audio stack (ALSA & JACK2)
sudo apt install -y libasound2-dev libjack-jackd2-dev jackd2 qjackctl

# UI frameworks & Network (Required by JUCE)
sudo apt install -y libwebkit2gtk-4.1-dev libgtk-3-dev libcurl4-openssl-dev

# SDL2 (Required by xwax)
sudo apt install -y libsdl2-dev

# Package tools (for creating DEB/RPM)
sudo apt install -y ruby ruby-dev build-essential rpm
gem install --user-install fpm

# Find your Ruby version and add to PATH
RUBY_VERSION=$(ls ~/.local/share/gem/ruby/ | head -1)
export PATH="$PATH:$HOME/.local/share/gem/ruby/$RUBY_VERSION/bin"
echo "export PATH=\"\$PATH:\$HOME/.local/share/gem/ruby/$RUBY_VERSION/bin\"" >> ~/.bashrc
source ~/.bashrc

# AppImage tools
sudo apt install -y appimagetool appimage-builder
```

#### Fedora

```bash
# Build tools
sudo dnf install gcc-c++ cmake make git ninja-build pkgconf-pkg-config

# Graphics and windowing
sudo dnf install libX11-devel libXext-devel libXrandr-devel mesa-libGL-devel freetype-devel

# Audio stack
sudo dnf install alsa-lib-devel jackd2 qjackctl jack-audio-connection-kit-devel

# UI frameworks, Network & SDL2
sudo dnf install gtk3-devel webkit2gtk4.1-devel libcurl-devel SDL2-devel

# Package tools
sudo dnf install ruby ruby-devel rpm-build
sudo gem install fpm

# AppImage tools
sudo dnf install appimagetool appimage-builder
```

---

## Building from Source

### Windows Build

#### Quick Release Build (Ninja)

```bash
cd VirtualDecks_warrior
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j%NUMBER_OF_PROCESSORS%
```

The executable will be located at: `build\juceDjApp_artefacts\Release\juceDjApp.exe` (or inside `build\juceDjApp_artefacts\` depending on generator).

#### Visual Studio Generator (Multi-Config)

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -- /m
```

Output: `build\juceDjApp_artefacts\Release\juceDjApp.exe`

#### Troubleshooting

- **FetchContent errors**: Ensure Git is installed and accessible in your system `PATH`.
- **Build cache errors**: Delete the `build/` folder and reconfigure.

---

### Linux Build

#### Quick Release Build (Ninja)

```bash
cd VirtualDecks_warrior
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)
```

The executable will be at: `build/juceDjApp_artefacts/juceDjApp` (or `build/juceDjApp_artefacts/Release/juceDjApp`).

#### Verify the Build

```bash
file build/juceDjApp_artefacts/Release/VirtualDecks
ldd build/juceDjApp_artefacts/Release/VirtualDecks  # Check runtime dependencies
./build/juceDjApp_artefacts/Release/VirtualDecks    # Test run
```

---

## Creating Distributable Packages

### Package Folder Structure

All release packages are organized in a `package/` folder at the project root:

```
package/
├── win/
│   ├── VirtualDecks-0.9.5-win64-portable/
│   │   ├── juceDjApp.exe
│   │   └── assets/
│   └── VirtualDecks-0.9.5-win64.exe           (NSIS installer)
└── linux/
    ├── VirtualDecks-0.9.5-x86_64/             (AppImage bundle)
    │   ├── VirtualDecks-0.9.5-x86_64.AppImage
    │   ├── logo.png
    │   └── virtualdecks.desktop
    ├── virtualdecks_0.9.5_amd64.deb           (Debian package)
    └── virtualdecks-0.9.5-1.el8.x86_64.rpm    (RPM package)
```

### Windows Packages

#### Portable ZIP

**Step 1: Build Release**

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**Step 2: Create Portable Directory**

```powershell
# Using PowerShell
mkdir -Force package\win\VirtualDecks-0.9.5-win64-portable
copy build\juceDjApp_artefacts\Release\VirtualDecks.exe package\win\VirtualDecks-0.9.5-win64-portable\
xcopy /E /I assets package\win\VirtualDecks-0.9.5-win64-portable\assets
```

**Step 3: Create ZIP**

```powershell
Compress-Archive -Path package\win\VirtualDecks-0.9.5-win64-portable\* -DestinationPath package\win\VirtualDecks-0.9.5-win64-portable.zip
```

---

#### NSIS Installer

The NSIS installer is created automatically via CPack during the build process on Windows.

**Step 1: Ensure NSIS is Installed**

```bash
# Verify NSIS is in PATH
makensis /VERSION
```

**Step 2: Configure and Build**

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**Step 3: Generate Installer via CPack**

```bash
cd build
cpack -G NSIS
```

Move the installer to the package folder:

```powershell
move VirtualDecks-0.9.5-win64.exe ..\package\win\
```

Output: `package/win/VirtualDecks-0.9.5-win64.exe`

**Step 4: (Optional) Manual NSIS Script**

Create `installer.nsi` in your project root:

```nsis
; VirtualDecks NSIS Installer
Name "VirtualDecks"
OutFile "VirtualDecks-Setup-0.9.5.exe"
InstallDir "$PROGRAMFILES64\VirtualDecks"

; Installer pages
Page directory
Page instfiles

; Uninstaller page
UninstPage uninstConfirm
UninstPage instfiles

Section "Install"
  SetOutPath "$INSTDIR\bin"
  File "build\juceDjApp_artefacts\Release\juceDjApp.exe"
  
  SetOutPath "$INSTDIR\assets"
  File /r "assets\*.*"
  
  ; Create Start Menu shortcuts
  CreateDirectory "$SMPROGRAMS\VirtualDecks"
  CreateShortCut "$SMPROGRAMS\VirtualDecks\VirtualDecks.lnk" "$INSTDIR\bin\juceDjApp.exe"
  CreateShortCut "$DESKTOP\VirtualDecks.lnk" "$INSTDIR\bin\juceDjApp.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\bin\juceDjApp.exe"
  Delete "$INSTDIR\assets\*.*"
  Delete "$SMPROGRAMS\VirtualDecks\VirtualDecks.lnk"
  Delete "$DESKTOP\VirtualDecks.lnk"
  RMDir /r "$INSTDIR"
SectionEnd
```

Compile with:

```bash
makensis installer.nsi
move VirtualDecks-Setup-0.9.5.exe package\win\
```

---

### Linux Packages

#### AppImage

**Step 1: Build Release**

```bash
cd VirtualDecks_warrior
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)
```

**Step 2: Create AppDir Structure**

```bash
mkdir -p package/linux/AppDir/usr/bin
mkdir -p package/linux/AppDir/usr/share/applications
mkdir -p package/linux/AppDir/usr/share/icons/hicolor/256x256/apps
mkdir -p package/linux/VirtualDecks-0.9.5-x86_64

# Copy executable (Check exact path depending on Ninja/Make)
cp build/juceDjApp_artefacts/Release/VirtualDecks package/linux/AppDir/usr/bin/

# Copy icon into hicolor theme structure
cp assets/logo.png package/linux/AppDir/usr/share/icons/hicolor/256x256/apps/virtualdecks.png
```

**Step 3: Create .desktop File**

Create `virtualdecks.desktop` (or copy if exists) to `package/linux/AppDir/usr/share/applications/`:

```bash
cp virtualdecks.desktop package/linux/AppDir/usr/share/applications/virtualdecks.desktop
```

**Step 4: Download & Run AppImage Tools**

```bash
mkdir -p package/linux/tools
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage
mv linuxdeploy-x86_64.AppImage package/linux/tools/

./package/linux/tools/linuxdeploy-x86_64.AppImage \
  --appdir package/linux/AppDir \
  --desktop-file package/linux/AppDir/usr/share/applications/virtualdecks.desktop \
  --icon-file package/linux/AppDir/usr/share/icons/hicolor/256x256/apps/virtualdecks.png \
  --output appimage
```

**Step 5: Move to Package Folder**

```bash
# Move AppImage and supporting files to release package folder
mv VirtualDecks-x86_64.AppImage package/linux/VirtualDecks-0.9.5-x86_64/
cp assets/logo.png package/linux/VirtualDecks-0.9.5-x86_64/virtualdecks.png
cp package/linux/AppDir/usr/share/applications/virtualdecks.desktop package/linux/VirtualDecks-0.9.5-x86_64/

# Zip the AppImage release folder
cd package/linux
zip -r VirtualDecks-0.9.5-x86_64-AppImage.zip VirtualDecks-0.9.5-x86_64
```

---

#### DEB Package (Using fpm)

```bash
# Create staging directory
mkdir -p /tmp/virtualdecks-deb-stage/usr/local/bin
mkdir -p /tmp/virtualdecks-deb-stage/usr/share/applications
mkdir -p /tmp/virtualdecks-deb-stage/usr/share/icons

# Copy files
cp build/juceDjApp_artefacts/Release/VirtualDecks /tmp/virtualdecks-deb-stage/usr/local/bin/
cp virtualdecks.desktop /tmp/virtualdecks-deb-stage/usr/share/applications/virtualdecks.desktop
cp assets/logo.png /tmp/virtualdecks-deb-stage/usr/share/icons/virtualdecks.png

# Create DEB
mkdir -p package/linux
fpm -s dir -t deb \
  -n virtualdecks \
  -v 0.9.5 \
  --architecture amd64 \
  --description "VirtualDecks DJ Application" \
  --url "[https://github.com/AlexsdeG/virtualdecks_warrior](https://github.com/AlexsdeG/virtualdecks_warrior)" \
  --license MIT \
  -C /tmp/virtualdecks-deb-stage \
  usr/local/bin/VirtualDecks \
  usr/share/applications/virtualdecks.desktop \
  usr/share/icons/virtualdecks.png

mv virtualdecks_0.9.5_amd64.deb package/linux/
rm -rf /tmp/virtualdecks-deb-stage
```

---

#### RPM Package (Using fpm)

```bash
# Create staging directory
mkdir -p /tmp/virtualdecks-rpm-stage/usr/local/bin
mkdir -p /tmp/virtualdecks-rpm-stage/usr/share/applications
mkdir -p /tmp/virtualdecks-rpm-stage/usr/share/icons

# Copy files
cp build/juceDjApp_artefacts/Release/VirtualDecks /tmp/virtualdecks-rpm-stage/usr/local/bin/VirtualDecks
cp virtualdecks.desktop /tmp/virtualdecks-rpm-stage/usr/share/applications/virtualdecks.desktop
cp assets/logo.png /tmp/virtualdecks-rpm-stage/usr/share/icons/virtualdecks.png

# Create RPM
mkdir -p package/linux
fpm -s dir -t rpm \
  -n virtualdecks \
  -v 0.9.5 \
  --architecture x86_64 \
  --description "VirtualDecks DJ Application" \
  --url "[https://github.com/AlexsdeG/virtualdecks_warrior](https://github.com/AlexsdeG/virtualdecks_warrior)" \
  --license MIT \
  -C /tmp/virtualdecks-rpm-stage \
  usr/local/bin/VirtualDecks \
  usr/share/applications/virtualdecks.desktop \
  usr/share/icons/virtualdecks.png

mv virtualdecks-0.9.5-1.x86_64.rpm package/linux/
rm -rf /tmp/virtualdecks-rpm-stage
```

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| FetchContent fails to download JUCE | Ensure Git is installed and accessible in your command prompt/terminal. |
| Missing libraries on Linux | Run `ldd build/juceDjApp_artefacts/juceDjApp` and install missing `-dev` packages. |
| Webkit2Gtk missing (Linux) | Ensure `libwebkit2gtk-4.1-dev` (Ubuntu) or `webkit2gtk4.1-devel` (Fedora) is installed. |
| xwax/SDL compilation errors | Ensure `libsdl2-dev` is installed. Check `CMakeLists.txt` fallback logic. |
| fpm not found on Linux | Run `gem install --user-install fpm` and ensure your `~/.local/share/gem/ruby/.../bin` is in `PATH`. |
