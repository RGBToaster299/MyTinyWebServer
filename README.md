# MyTinyWebServer
![Status](https://img.shields.io/badge/status-mostly%20working-yellow)
![License](https://img.shields.io/badge/license-CC--BY--NC--ND%204.0-lightgrey)

**MTWS** is a small but useful web server for small projects.  
(Not intended for *real web hosting* — for that, I recommend [Nginx](https://nginx.org).)

---

## How to Compile

A compiled version is already included.  
However, if you've made changes and want to compile it yourself, follow these steps.

(*If you don't want to compile it, just download the `mtws.cpp` file and place it in your root web folder. Then skip to Step 3.*)

### 1. Setup

Make sure you have the **GNU C++ Compiler** installed.

#### Debian/Ubuntu-based:
```bash
sudo apt install g++
```

#### Arch-Based:
```bash
sudo pacman -S gcc
```

#### Fedora/RHEL/CentOS:
```bash
sudo dnf install gcc-c++
```

#### openSUSE:
```bash
sudo zypper install gcc-c++
```

#### Windows:
- Install [MSYS2](https://www.msys2.org/)
- Then run: `pacman -S mingw-w64-x86_64-gcc`

#### macOS:
- Install Xcode Command Line Tools:
```bash
xcode-select --install
```

---

### 2. Compiling

Clone the repository:
```bash
git clone https://github.com/RGBToaster299/MyTinyWebServer.git
```

Navigate into the new folder:
```bash
cd MyTinyWebServer/
```

Compile the project:
```bash
g++ -std=c++17 -o mlws mlws.cpp -ldl
```

You should now have a compiled binary named `mlws`.

---

## 3. Running the Server

1. Put the `mlws` binary into your web folder  
   *(this folder will be used as the root directory for your website)*.
2. Open your terminal (if not already open).
3. Run the server:
```bash
./mlws -p 8080
```

The server should now be running.

### Stopping the Server

Type `stop` into the console.

---

## Plugins?

Yes, plugin support is planned for MTWS.  
However, I'm currently focusing on improving the stability of the core project.

---

## Explorer

MTWS includes a built-in file explorer.  
It is shown when the current directory doesn't contain an `index.html` (or any `.html`) file.

---

## Port 80 Issues

Port 80 may not work on some systems — this is likely due to:

- Permission issues (requires root/admin on many OSes)
- Another process already using the port

This is a known issue, and I'm working on resolving it.  
If you manage to fix it, feel free to contribute via pull request or commit!

---

## Supported Platforms

MTWS runns as expected **on Linux**.
Untested on Windows or MacOS.

---

## License

This project is licensed under the  
**Creative Commons Attribution-NonCommercial-NoDerivatives 4.0 International License**.  
See [license.txt](./license.txt) for full legal terms or visit the [official license page](https://creativecommons.org/licenses/by-nc-nd/4.0/legalcode).
