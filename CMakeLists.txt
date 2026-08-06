cmake_minimum_required(VERSION 3.21)
project(LibraryManagementSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)
qt_standard_project_setup()

qt_add_executable(LibraryApp WIN32
    src/main.cpp
    src/models/Entity.h
    src/models/Book.h
    src/models/Book.cpp
    src/models/Reader.h
    src/models/Reader.cpp
    src/models/BorrowRecord.h
    src/models/BorrowRecord.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/managers/BookManager.h
    src/managers/BookManager.cpp
    src/managers/ReaderManager.h
    src/managers/ReaderManager.cpp
    src/managers/LibrarySystem.h
    src/managers/LibrarySystem.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/DashboardPage.h
    src/ui/DashboardPage.cpp
    src/ui/BooksPage.h
    src/ui/BooksPage.cpp
    src/ui/ReadersPage.h
    src/ui/ReadersPage.cpp
    src/ui/BorrowPage.h
    src/ui/BorrowPage.cpp
)

target_include_directories(LibraryApp PRIVATE src)
target_link_libraries(LibraryApp PRIVATE Qt6::Widgets Qt6::Sql)
