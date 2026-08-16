cmake_minimum_required(VERSION 3.21)
project(LibraryManagementSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)
qt_standard_project_setup()

qt_add_executable(LibraryApp WIN32
    src/main.cpp
    src/i18n/Lang.h
    src/i18n/Lang.cpp
    src/ui/Exporter.h
    src/ui/Exporter.cpp
    src/util/Payment.h
    src/util/Payment.cpp
    src/util/QrCode.h
    src/util/QrCode.cpp
    src/ui/PaymentDialog.h
    src/ui/PaymentDialog.cpp
    src/util/SearchUtil.h
    src/util/SearchUtil.cpp
    src/util/AvatarUtil.h
    src/util/AvatarUtil.cpp
    src/ui/ProfileDialog.h
    src/ui/ProfileDialog.cpp
    src/ui/ReaderDetailDialog.h
    src/ui/ReaderDetailDialog.cpp
    src/ui/SlipDialog.h
    src/ui/SlipDialog.cpp
    src/ui/SuggestBox.h
    src/ui/SuggestBox.cpp
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/models/Entity.h
    src/models/Book.h
    src/models/Book.cpp
    src/models/Reader.h
    src/models/Reader.cpp
    src/models/BorrowRecord.h
    src/models/BorrowRecord.cpp
    src/models/Account.h
    src/models/Account.cpp
    src/models/Reservation.h
    src/models/Reservation.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/managers/BookManager.h
    src/managers/BookManager.cpp
    src/managers/ReaderManager.h
    src/managers/ReaderManager.cpp
    src/managers/AccountManager.h
    src/managers/AccountManager.cpp
    src/managers/LibrarySystem.h
    src/managers/LibrarySystem.cpp
    src/ui/LoginDialog.h
    src/ui/LoginDialog.cpp
    src/ui/PasswordDialog.h
    src/ui/PasswordDialog.cpp
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
    src/ui/AdminPage.h
    src/ui/AdminPage.cpp
    src/ui/RecordsDialog.h
    src/ui/RecordsDialog.cpp
    src/ui/MyLoansPage.h
    src/ui/MyLoansPage.cpp
)

target_include_directories(LibraryApp PRIVATE src)
target_link_libraries(LibraryApp PRIVATE Qt6::Widgets Qt6::Sql)

# ---------------------------------------------------------------- tests
# Headless checks for the business rules. Off by default so a normal build
# stays quick; turn on with -DBUILD_TESTING=ON.
option(BUILD_TESTING "Build the LibraryTests executable" OFF)

if(BUILD_TESTING)
    enable_testing()

    # Everything except the GUI: the rules under test live below src/ui.
    set(LIBRARY_CORE_SOURCES
        src/i18n/Lang.cpp
        src/util/SearchUtil.cpp
        src/util/QrCode.cpp
        src/util/Payment.cpp
        src/models/Book.cpp
        src/models/Reader.cpp
        src/models/BorrowRecord.cpp
        src/models/Account.cpp
        src/models/Reservation.cpp
        src/database/DatabaseManager.cpp
        src/managers/BookManager.cpp
        src/managers/ReaderManager.cpp
        src/managers/AccountManager.cpp
        src/managers/LibrarySystem.h
        src/managers/LibrarySystem.cpp
    )

    qt_add_executable(LibraryTests
        tests/LibraryTests.cpp
        ${LIBRARY_CORE_SOURCES}
    )
    target_include_directories(LibraryTests PRIVATE src)
    # Gui for QImage/QPainter (the QR renderer), Widgets for the dialog statics.
    target_link_libraries(LibraryTests PRIVATE Qt6::Core Qt6::Sql Qt6::Gui Qt6::Widgets)

    add_test(NAME library-rules COMMAND LibraryTests)
endif()
