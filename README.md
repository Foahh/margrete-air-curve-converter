# Air-Curve Converter Plugin for Margrete

Convert **Curve** patterns to  
`Slide`, `AirSlide`, and `AirCrush` notes – with optional import from `.aff` charts.

---

## Usage

- See the [wiki](https://github.com/Foahh/margrete-air-curve-converter/wiki) for detailed usage instructions.

- 详细使用说明请参见 [wiki](https://github.com/Foahh/margrete-air-curve-converter/wiki)。

- 詳細な使用方法については [wiki](https://github.com/Foahh/margrete-air-curve-converter/wiki) をご覧ください。

---

## Build

### 1. Clone

```console
git clone https://github.com/Foahh/margrete-air-curve-converter
cd margrete-air-curve-converter
```

### 2. Install Prerequisites

| Tool                            | How to get it                                                                                                                                                                                                   |
|---------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **MSVC (Microsoft Visual C++)** | • Launch **Visual Studio Installer**.<br>• Select the **Desktop development with C++** workload.<br>  - *or* -<br>• Install **MSVC Build Tools** + **Windows SDK** only (no full VS IDE required).              |
| **vcpkg**                       | • In **Visual Studio Installer**, tick the **vcpkg** component.<br>  - *or* -<br>• Install manually through [Tutorial](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-powershell) |

### 3. Run the build script

1. Open **PowerShell**.
2. Navigate to the project root.
3. Execute:

```powershell
./build.ps1 -b -p release   
```

- `-b` tells the script to **build**.
- `-p` chooses the **build profile** (`Debug` or `Release`).

### 4. Locate the output

The compiled DLL is placed in:

```txt
out/build/<profile>/
```

where `<profile>` is `Debug` or `Release`, matching the profile you selected.

## Development

- C++20 / CMake ≥ 3.30
- Code style: clang-format (`.clang-format` in repo)

Issues & Pull Requests are welcome!

---

## License

See [`LICENSE`](LICENSE) for details.
