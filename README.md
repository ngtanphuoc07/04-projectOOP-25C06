# Library Management System (C++ / Qt 6 / SQLite)

OOP course project: a desktop library management application implementing the
class design from `UML.pdf` and the functional specification from
`DesignSystem.pdf`.

![Language](https://img.shields.io/badge/C%2B%2B-17-blue) ![GUI](https://img.shields.io/badge/GUI-Qt%206-green) ![DB](https://img.shields.io/badge/DB-SQLite-lightgrey)

## Features

- **Dashboard** — live statistics (total copies, registered readers, copies
  borrowed, copies on shelf) and a table of books currently out on loan.
- **Books** — add / update / delete / list books, search by title, input
  validation, detail panel.
- **Readers** — add / update / delete / list readers, search by name, phone and
  email validation, detail panel.
- **Borrow / Return** — create borrow records (availability check, per-reader
  limit of 5 books, duplicate-loan check), return by record, filter history by
  status. Stock and reader borrow counts update automatically.
- **Persistence** — SQLite database `library.db` created next to the executable
  on first run and seeded with sample data (5 books, 3 readers).

---

## 1. Prerequisites

| Tool | Version | Where to get it |
|---|---|---|
| Qt (MinGW 64-bit kit) | 6.x (tested with 6.11.1) | [Qt Online Installer](https://www.qt.io/download-qt-installer) |
| MinGW toolchain | 13.1.0 (ships with Qt) | Qt Online Installer → Build Tools |
| CMake | ≥ 3.21 (ships with Qt) | Qt Online Installer → Build Tools |
| Ninja | any (ships with Qt) | Qt Online Installer → Build Tools |

### Installing Qt (one time, ~10 minutes)

1. Download the **Qt Online Installer** and sign in with a free Qt account.
2. Keep the default install location **`C:\Qt`** — every path in this project
   assumes it.
3. Choose **Custom installation** and tick:
   - **Qt → Qt 6.x.x → MinGW 64-bit** (the framework itself)
   - **Build Tools (Developer Tools) → MinGW 64-bit** (the compiler)
   - **Build Tools → CMake**
   - **Build Tools → Ninja**
4. After installation, verify these paths exist (adjust `6.11.1` /
   `mingw1310_64` to whatever versions you installed):
   - `C:\Qt\6.11.1\mingw_64\`
   - `C:\Qt\Tools\mingw1310_64\bin\g++.exe`
   - `C:\Qt\Tools\CMake_64\bin\cmake.exe`
   - `C:\Qt\Tools\Ninja\ninja.exe`

> **Installed a different Qt version?** Search & replace the two path fragments
> `C:/Qt/6.11.1/mingw_64` and `C:/Qt/Tools/mingw1310_64` in
> `.vscode/settings.json`, `.vscode/tasks.json`, `.vscode/launch.json`,
> `run.bat`, and the commands below.

---

## 2. Building

### Option A — VS Code (recommended)

1. Install the extensions **C/C++** (`ms-vscode.cpptools`) and **CMake Tools**
   (`ms-vscode.cmake-tools`).
2. **File → Open Folder** → select this `Library` folder (the one containing
   `CMakeLists.txt` — the title bar must read "Library"). The `.vscode` folder
   already configures the compiler, CMake, Ninja and Qt paths.
3. If prompted *"Select a kit"* → choose **[Unspecified]** (the compiler is
   already pinned in settings).
4. Build: **`Ctrl+Shift+B`** — success looks like
   `Linking CXX executable LibraryApp.exe`.
5. Run / debug: **`F5`** (breakpoints work; Qt DLLs are put on `PATH`
   automatically by `.vscode/launch.json`).

Do **not** use the "Build" button contributed by other extensions in the status
bar — only `Ctrl+Shift+B` / `F5` use this project's configuration.

### Option B — Command line

```bat
set PATH=C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%

cmake -S . -B build -G Ninja ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 ^
      -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe

cmake --build build
```

Then start the app with `run.bat` (it puts the Qt `bin` folder on `PATH` before
launching `build\LibraryApp.exe`).

### Option C — Qt Creator

Open `CMakeLists.txt` in Qt Creator, accept the default Desktop MinGW kit,
press **Run**. No extra configuration needed.

---

## 3. Distributing to a machine without Qt

A bare `LibraryApp.exe` will not start on a machine without Qt installed
(missing `Qt6Core.dll`, etc.). Create a self-contained folder with
`windeployqt`:

```bat
mkdir LibraryApp-portable
copy build\LibraryApp.exe LibraryApp-portable\
C:\Qt\6.11.1\mingw_64\bin\windeployqt.exe --release --compiler-runtime LibraryApp-portable\LibraryApp.exe
```

Zip `LibraryApp-portable` and share it — the recipient just extracts and runs
`LibraryApp.exe`. The folder includes the Qt DLLs, the MinGW runtime and the
SQLite driver plugin (`sqldrivers\qsqlite.dll`).

---

## 4. Project structure

```
CMakeLists.txt
.vscode/                         ready-made VS Code build/run/debug config
src/
├── main.cpp                     application entry point
├── models/
│   ├── Entity.h                 ABSTRACT base class (abstraction)
│   ├── Book.h/.cpp              book entity
│   ├── Reader.h/.cpp            reader entity
│   └── BorrowRecord.h/.cpp      borrow transaction entity
├── database/
│   └── DatabaseManager.h/.cpp   SQLite access — SINGLETON pattern
├── managers/
│   ├── BookManager.h/.cpp       book business rules
│   ├── ReaderManager.h/.cpp     reader business rules
│   └── LibrarySystem.h/.cpp     FACADE — borrow/return workflow + statistics
└── ui/
    ├── MainWindow.h/.cpp        tabbed main window + stylesheet
    ├── DashboardPage.h/.cpp     statistics / report screen
    ├── BooksPage.h/.cpp         book CRUD screen
    ├── ReadersPage.h/.cpp       reader CRUD screen
    └── BorrowPage.h/.cpp        borrow / return screen
```

## 5. OOP principles

| Principle | Where |
|---|---|
| **Encapsulation** | Every model keeps its state `private` and exposes getters/setters (`Book`, `Reader`, `BorrowRecord`). |
| **Inheritance** | `Book`, `Reader`, `BorrowRecord` all derive from the common base `Entity`. |
| **Abstraction** | `Entity` is an abstract class: `getID()` and `displayInfo()` are pure virtual. |
| **Polymorphism** | The detail panels call `Entity::displayInfo()` through an `Entity&` — the method chosen at runtime depends on the concrete type (see `BooksPage::showDetails`, `ReadersPage::showDetails`, `BorrowPage::onRowSelected`). |

## 6. Design patterns

1. **Singleton** — `DatabaseManager` (`src/database/DatabaseManager.h`). One
   shared SQLite connection for the whole application; private constructor,
   deleted copy operations, access only via `DatabaseManager::instance()`.
2. **Facade** — `LibrarySystem` (`src/managers/LibrarySystem.h`). The GUI calls
   one method (`borrowBook` / `returnBook`) and the facade coordinates the
   multi-step workflow across `DatabaseManager`, `BookManager`, `ReaderManager`
   (validate → create record → adjust stock → adjust reader count → persist).
3. **Observer** — Qt signals/slots. `LibrarySystem::dataChanged()` is emitted
   after every mutation; all four pages subscribe and refresh automatically, so
   the dashboard is always consistent with the data screens.

## 7. Business logic beyond CRUD

`LibrarySystem::borrowBook` enforces: reader exists, book exists, reader is
under the 5-book limit, at least one copy is available, and the reader is not
already holding the same title. `returnBook` rejects double returns and return
dates before the borrow date. `BookManager`/`ReaderManager` refuse to delete
books still on loan or readers still holding books, and keep
`availableQuantity` consistent when the total quantity is edited.

---

## 8. Troubleshooting

| Symptom | Cause → fix |
|---|---|
| "Bad CMake executable" in VS Code | Wrong folder opened or wrong Qt paths → open the `Library` folder itself; fix versions in `.vscode/*.json` |
| App won't start: missing `Qt6Core.dll` | Qt `bin` not on `PATH` → launch via `F5` or `run.bat`, or deploy with `windeployqt` (section 3) |
| Linker: `Permission denied: LibraryApp.exe` | The app is still running → close its window and rebuild |
| `msbuild is not recognized` | You clicked another extension's Build button → use `Ctrl+Shift+B` |
| Red squiggles under `#include <Q...>` but the build works | IntelliSense not configured yet → `Ctrl+Shift+P` → "CMake: Delete Cache and Reconfigure" |
| "Select a kit" prompt | Choose **[Unspecified]** — the compiler is pinned in `.vscode/settings.json` |
| Reset the data | Delete `library.db` next to the executable; it is recreated and reseeded on next start |
