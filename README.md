# Library Management System (C++ / Qt 6 / SQLite)

OOP course project: a desktop library management application implementing the
class design from `UML.pdf` and the functional specification from
`DesignSystem.pdf`.

![Language](https://img.shields.io/badge/C%2B%2B-17-blue) ![GUI](https://img.shields.io/badge/GUI-Qt%206-green) ![DB](https://img.shields.io/badge/DB-SQLite-lightgrey)

## Accounts

The application opens with a sign-in dialog. Two roles exist.

| Role | Demo account | Sees |
|---|---|---|
| **Librarian** | `admin` / `123` | Dashboard, Books, Readers, Borrow / Return |
| **Reader** | `an` / `reader123` | My books, Browse catalogue (borrow, reserve) |

A visitor can create their own reader account from the **Register** tab; the
matching `Reader` row is created in the same step. Passwords are never stored:
only a **PBKDF2-HMAC-SHA256** digest with 120,000 iterations and a random salt
per account. Five wrong guesses lock an account for five minutes. A librarian
can reset a forgotten password from **Manage → Accounts**; there is no self-service
recovery by design.

> `admin` / `123` is a demo credential that deliberately breaks the password
> rules new accounts must follow (6+ characters, a letter and a digit). Change
> it from **Manage → Accounts** before using this for anything real.

> The `admin` account is created on every start if the `Accounts` table is
> empty, so it always exists. The `an` demo reader belongs to the sample data,
> which is only inserted into a **brand-new** `library.db` — if you upgraded an
> existing database, register your own reader account instead.

## Features

- **Sign in / register** — role-based access, lockout after repeated failures,
  librarian password reset, sign out and switch user without restarting.
- **Light and dark themes**, switched from the header and remembered between runs.
- **Renewals** (2 × 7 days, blocked when overdue or when somebody is queued),
  **reservations** with a first-come-first-served queue that promotes the next
  reader automatically when a copy comes back, and **cancel loan** to undo a
  record entered by mistake.
- **Fines that can be settled** — collect part or all of a fee, or waive it.
  A balance that reaches zero stays at zero, so an old late return does not
  follow a reader forever.
- **Manage screen** (librarian) — accounts, the lending rules, and an audit log
  of every change with the account that made it.
- **CSV export** from the dashboard and from any history view.
- **English / Vietnamese** — switch language from the header at any time; the
  choice is remembered between runs (`QSettings`). Every screen, dialog and
  error message follows it.
- **Overdue and unreturned items** — a reader holding an overdue book cannot
  borrow again until it comes back, late fees accrue at 5,000 ₫ per day, and a
  loan more than 30 days past its deadline is reported separately as *not
  returned*. The fee is shown to the librarian at the moment the book is
  returned.
- **Dashboard** — live statistics (total copies, registered readers, copies
  borrowed, copies on shelf, **overdue**) and a table of books currently out on
  loan with their due dates; late loans are highlighted.
- **Books** — add / update / delete / list books, search across title, author,
  category and ID, sortable columns, auto-generated next ID, detail panel, and
  **the full borrow history of any selected book**.
- **Readers** — add / update / delete / list readers, search across name, ID,
  phone and email, sortable columns, auto-generated next ID, and **the books a
  selected reader is currently holding plus their full history**. Readers
  holding an overdue book are flagged in the list.
- **Borrow / Return** — create borrow records (availability check, per-reader
  limit of 5 books, duplicate-loan check), return by record, filter history by
  status including *Overdue*. Stock and reader borrow counts update
  automatically.
- **Due dates** — every loan gets a deadline of borrow date + 14 days. Overdue
  loans are counted on the dashboard and shown in red wherever they appear.
- **Patron interface** (reader role) — **My books** shows what they are
  holding, when each item is due and what they owe; **Browse catalogue** shows
  copies, author and category and lets them borrow a title themselves. A patron
  can never see *who* borrowed a book: the borrow-history button does not exist
  in their build of the screen, and the reader list is not among their tabs.
- **Configurable rules** — borrow limit, loan period, fine per day, lost-after
  threshold and renewal allowance live in a `Settings` table and are edited in
  the app, not recompiled.
- **Persistence** — SQLite database `library.db` created next to the executable
  on first run and seeded with sample data (5 books, 3 readers, 2 accounts).
  An older `library.db` is upgraded in place — `dueDate`, `finePaid`,
  `renewCount`, the lockout columns and the `Accounts`, `Reservations`,
  `Settings` and `AuditLog` tables are all added automatically, so no data is
  lost. Multi-step writes run inside a transaction, so a failure half way
  through cannot leave stock and records disagreeing.

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

### Running the tests

71 headless checks over the business rules — accounts, hashing, lockout,
borrowing, due dates, fines, renewals, cancellation, statistics, config and
translation. They use a throwaway database in the build folder and never touch
the real one.

```bat
cmake -S . -B build-tests -G Ninja -DBUILD_TESTING=ON ^
      -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 ^
      -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build-tests --target LibraryTests
build-tests\LibraryTests.exe
```

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
├── main.cpp                     entry point + sign-in loop
├── i18n/
│   └── Lang.h/.cpp              English/Vietnamese lookup + languageChanged
├── models/
│   ├── Entity.h                 ABSTRACT base class (abstraction)
│   ├── Book.h/.cpp              book entity
│   ├── Reader.h/.cpp            reader entity
│   ├── BorrowRecord.h/.cpp      borrow transaction (due date, fine, renewals)
│   ├── Account.h/.cpp           login account (role, linked reader, lockout)
│   └── Reservation.h/.cpp       a reader queued for an unavailable title
├── database/
│   └── DatabaseManager.h/.cpp   SQLite access — SINGLETON pattern
├── managers/
│   ├── BookManager.h/.cpp       book business rules
│   ├── ReaderManager.h/.cpp     reader business rules
│   ├── AccountManager.h/.cpp    credentials, hashing, username rules
│   └── LibrarySystem.h/.cpp     FACADE — borrow/return, registration, stats
└── ui/
    ├── Theme.h/.cpp             every colour and style rule, light and dark
    ├── Exporter.h/.cpp          "save what the table shows" as CSV
    ├── AdminPage.h/.cpp         accounts, lending rules and the audit log
    ├── LoginDialog.h/.cpp       sign in / register gate
    ├── MainWindow.h/.cpp        role-based tabs + user bar + stylesheet
    ├── DashboardPage.h/.cpp     statistics / report screen
    ├── BooksPage.h/.cpp         book CRUD screen (read-only for readers)
    ├── ReadersPage.h/.cpp       reader CRUD screen
    ├── BorrowPage.h/.cpp        borrow / return screen
    ├── RecordsDialog.h/.cpp     reusable history viewer (per book / per reader)
    └── MyLoansPage.h/.cpp       the signed-in reader's own loans
```

## 5. OOP principles

| Principle | Where |
|---|---|
| **Encapsulation** | Every model keeps its state `private` and exposes getters/setters (`Book`, `Reader`, `BorrowRecord`, `Account`). `Account` never exposes a plain password. |
| **Inheritance** | `Book`, `Reader`, `BorrowRecord` and `Account` all derive from the common base `Entity`. |
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
already holding the same title. It also stamps the loan with a due date of
borrow date + 14 days. `returnBook` rejects double returns and return dates
before the borrow date. `BookManager`/`ReaderManager` refuse to delete books
still on loan or readers still holding books, and keep `availableQuantity`
consistent when the total quantity is edited.

`LibrarySystem::registerReaderAccount` spans two tables, so it is the facade's
job: it validates the username and password, creates the `Reader` row, then the
linked `Account` — and deletes the reader again if the account insert fails, so
a half-finished registration cannot be left behind.

`AccountManager::login` returns the same message for an unknown username and a
wrong password, so the dialog gives no clue about which accounts exist.

### Overdue, fines and unreturned items

| Rule | Where |
|---|---|
| A reader holding an overdue book cannot borrow again | `LibrarySystem::borrowBook` (checked before anything is written) |
| Late fee = days late × 5,000 ₫ | `BorrowRecord::fine` |
| The fee stops growing once the book is back | `BorrowRecord::lateDays` measures to the **return date** for closed records, to today for open ones |
| A loan 30+ days past its deadline is reported as *not returned* | `BorrowRecord::isLost`, `LibrarySystem::countLostBooks` |
| The fee is shown when the book is handed back | `LibrarySystem::returnBook` reports it through `fineOut` |

Fees are reported, never stored: the system has no notion of payment, so
recording an amount it cannot mark as settled would be misleading.

### What a patron may see

`BooksPage` takes `editable` and `selfReaderID`. In patron mode the edit form
is hidden, a **Borrow this book** button appears, and the borrow-history button
is not created at all — so the list of who else borrowed a title is not merely
hidden but absent. Patrons also get no *Readers* tab. Every borrow they make
goes through the same `LibrarySystem::borrowBook` as the librarian's, so the
limit, availability, duplicate-title and overdue rules apply identically.

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
| Reset the data | Delete `library.db` next to the executable; it is recreated and reseeded on next start (including the `admin` account) |
| Forgot the librarian password | There is no recovery by design. Delete `library.db` to start over, or clear the `Accounts` table with a SQLite browser and restart — `admin` / `123` is recreated |
| Signed in as the wrong user | **Sign out** in the dark bar at the top returns to the login dialog without closing the app |
