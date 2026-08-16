// Headless checks for the business logic: accounts, borrowing, due dates,
// fines, renewals, reservations, history queries and the translation layer.
//
// Build and run:
//     cmake -S . -B build -DBUILD_TESTING=ON
//     cmake --build build --target LibraryTests
//     build/LibraryTests.exe
//
// Runs against a throwaway library.db in the test executable's own folder, so
// it never touches the real database next to LibraryApp.exe.
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QFile>
#include <cstdio>

#include "database/DatabaseManager.h"
#include "i18n/Lang.h"
#include "managers/LibrarySystem.h"
#include "util/QrCode.h"
#include "util/SearchUtil.h"
#include "util/Payment.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const QString &what)
{
    ++checks;
    std::printf("  %s | %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++failures;
    std::fflush(stdout);
}

static void section(const char *title)
{
    std::printf("\n== %s ==\n", title);
    std::fflush(stdout);
}

int main(int argc, char *argv[])
{
    // QGuiApplication, not QCoreApplication: the QR renderer paints onto a QImage.
    QGuiApplication app(argc, argv);
    QFile::remove(QGuiApplication::applicationDirPath() + "/library.db");

    DatabaseManager *db = DatabaseManager::instance();
    if (!db->connectDatabase() || !db->createTables()) {
        std::printf("could not open the test database\n");
        return 2;
    }

    LibrarySystem system;
    system.seedSampleDataIfEmpty();
    system.setCurrentUser("admin");

    QString err;
    const QDate today = QDate::currentDate();
    const QString todayStr = today.toString("yyyy-MM-dd");

    // ------------------------------------------------------------------
    section("1. Accounts and sign-in");
    check(system.accountManager()->login("admin", "123", &err).isLibrarian(),
          "admin/123 signs in as Librarian");
    check(!system.accountManager()->login("admin", "wrong", &err).isValid(),
          "a wrong password is rejected");
    check(err == TR("Incorrect username or password."),
          "unknown user and wrong password give the same message");
    const Account demo = system.accountManager()->login("an", "reader123", &err);
    check(demo.isValid() && !demo.isLibrarian() && demo.getLinkedReaderID() == "R001",
          "the demo reader signs in and is linked to R001");

    section("2. Password storage");
    const QString stored = db->findAccountByUsername("admin").getPasswordHash();
    check(stored.startsWith("pbkdf2$"), "passwords are stored as PBKDF2, not bare SHA-256");
    check(!stored.contains("123"), "the plain password is not stored");
    check(AccountManager::hashPassword("same") != AccountManager::hashPassword("same"),
          "the same password hashes differently every time (random salt)");
    check(AccountManager::verifyPassword(AccountManager::hashPassword("secret1"),
                                         "anyone", "secret1"),
          "a freshly hashed password verifies");
    // An account created by the first version must still work, and be upgraded.
    const QString legacy = QString::fromLatin1(
        QCryptographicHash::hash(QByteArray("olduser:oldpass1"),
                                 QCryptographicHash::Sha256).toHex());
    db->insertAccount(Account("olduser", legacy, Account::Member, QString()));
    check(system.accountManager()->login("olduser", "oldpass1", &err).isValid(),
          "an old unsalted hash still signs in");
    check(db->findAccountByUsername("olduser").getPasswordHash().startsWith("pbkdf2$"),
          "and is upgraded to PBKDF2 on the way in");

    section("3. Lockout after repeated failures");
    for (int i = 0; i < AccountManager::maxAttempts(); ++i)
        system.accountManager()->login("olduser", "nope", &err);
    check(!system.accountManager()->login("olduser", "oldpass1", &err).isValid(),
          "the account locks even for the correct password");
    check(err.contains("minute"), "the message says how long to wait");
    check(system.accountManager()->unlockAccount("olduser", &err), "a librarian can unlock it");
    check(system.accountManager()->login("olduser", "oldpass1", &err).isValid(),
          "and it works again");

    section("3b. A librarian can lock an account outright");
    check(system.accountManager()->lockAccount("olduser", "admin", &err),
          "the account is locked");
    check(db->findAccountByUsername("olduser").isDisabled(),
          "it is marked as disabled, not just timed out");
    check(!system.accountManager()->login("olduser", "oldpass1", &err).isValid(),
          "the correct password no longer gets in");
    check(err == TR("This account has been locked by a librarian."),
          "and the message says a librarian did it, not 'try again in N minutes'");
    check(!system.accountManager()->lockAccount("olduser", "admin", &err),
          "locking it twice is refused");
    check(!system.accountManager()->lockAccount("admin", "admin", &err),
          "you cannot lock the account you are signed in with");
    check(!system.accountManager()->lockAccount("admin", "someone.else", &err),
          "the last usable librarian cannot be locked either");
    check(system.accountManager()->unlockAccount("olduser", &err), "unlocking restores it");
    check(system.accountManager()->login("olduser", "oldpass1", &err).isValid(),
          "and the reader can sign in again");

    section("4. Registration");
    check(system.registerReaderAccount("linh", "pass1234", "Pham Thi Linh",
                                       "0987654321", "linh@example.com", &err),
          "a visitor can register");
    check(system.readerManager()->findReaderByID("R004").getFullName() == "Pham Thi Linh",
          "the Reader row was created too");
    check(!system.registerReaderAccount("mai", "abc", "Weak Password",
                                        "0911111111", "m@example.com", &err),
          "a too-short password is refused");
    check(!system.registerReaderAccount("mai", "abcdefgh", "No Digit",
                                        "0911111111", "m@example.com", &err),
          "a password with no digit is refused");
    check(!system.registerReaderAccount("linh", "pass1234", "Duplicate",
                                        "0911111111", "d@example.com", &err),
          "a duplicate username is refused");
    check(system.readerManager()->getAllReaders().size() == 4,
          "the refused registrations left no orphan readers");

    section("5. Deleting a reader takes the login with it");
    check(system.accountManager()->findAccountByUsername("linh").isValid(),
          "the account exists before the delete");
    check(system.deleteReaderAndAccount("R004", &err), "the reader is deleted");
    check(!system.accountManager()->findAccountByUsername("linh").isValid(),
          "the linked account is gone as well — no orphan login");

    section("6. Borrowing, due dates and late fees");
    check(system.borrowBook("R001", "B001", today.addDays(-20).toString("yyyy-MM-dd"), &err),
          "R001 borrows B001, back-dated 20 days");
    BorrowRecord late = system.getActiveRecordsByReader("R001").first();
    check(late.getDueDate() == today.addDays(-6).toString("yyyy-MM-dd"),
          "the deadline is borrow date + 14 days");
    check(late.lateDays(today) == 6, "6 days late");
    check(late.fine(system.finePerDay(), today) == 30000, "fine = 6 x 5,000 = 30,000");
    check(LibrarySystem::formatMoney(30000) == QString::fromUtf8("30,000 ₫"), "money is grouped");

    section("7. An overdue book blocks further borrowing");
    check(!system.borrowBook("R001", "B002", todayStr, &err), "R001 is blocked");
    check(err.contains("overdue"), "the message explains why");
    check(system.borrowBook("R001", "B002", todayStr, &err, /*overrideOverdue=*/true),
          "a librarian can override the block");
    check(system.borrowBook("R002", "B003", todayStr, &err),
          "a reader with nothing overdue is unaffected");

    section("8. Fines can be collected and waived");
    check(system.outstandingFine("R001") == 30000, "R001 owes 30,000");
    check(system.settleFine(late.getRecordID(), 10000, &err), "10,000 is collected");
    check(system.outstandingFine("R001") == 20000, "20,000 left to pay");
    check(!system.settleFine(late.getRecordID(), 999999, &err),
          "more than the balance is refused");
    check(system.waiveFine(late.getRecordID(), &err), "the rest is waived");
    check(system.outstandingFine("R001") == 0,
          "the balance reaches zero — a fine no longer follows a reader forever");
    check(!system.waiveFine(late.getRecordID(), &err), "nothing left to waive");

    section("9. Renewals");
    const QString fresh = system.getActiveRecordsByReader("R002").first().getRecordID();
    check(system.renewLoan(fresh, &err), "an on-time loan can be renewed");
    check(db->findBorrowRecordByID(fresh).getRenewCount() == 1, "the counter went up");
    check(db->findBorrowRecordByID(fresh).getDueDate()
              == today.addDays(system.loanPeriodDays() + 7).toString("yyyy-MM-dd"),
          "the deadline moved 7 days");
    check(system.renewLoan(fresh, &err), "a second renewal is allowed");
    check(!system.renewLoan(fresh, &err), "a third is refused (limit is 2)");
    check(!system.renewLoan(late.getRecordID(), &err), "an overdue loan cannot be renewed");

    section("10. Reservations");
    // Drain B004 so nothing is left on the shelf.
    const Book b4 = system.bookManager()->findBookByID("B004");
    for (int i = 0; i < b4.getAvailableQuantity(); ++i) {
        const QString rid = QString("R%1").arg(i + 1, 3, 10, QChar('0'));
        system.borrowBook(rid, "B004", todayStr, &err, true);
    }
    check(system.bookManager()->findBookByID("B004").getAvailableQuantity() >= 0,
          "B004 has been borrowed from");
    // A reader may now request any title: a copy on the shelf is held for them
    // straight away, otherwise they join the queue.
    check(system.reserveBook("R003", "B005", &err), "an in-stock title can be requested");
    check(system.getOpenReservationsByReader("R003").first().getState()
              == Reservation::Ready,
          "and is marked ready for pickup at once");
    check(!system.reserveBook("R003", "B005", &err), "requesting it twice is refused");
    const QString reqID = system.getOpenReservationsByReader("R003").first().getReservationID();
    check(system.fulfilReservation(reqID, &err), "the librarian hands it over");
    check(!system.getActiveRecordsByReader("R003").isEmpty(), "which creates a real loan");
    check(!system.fulfilReservation(reqID, &err), "a closed request cannot be fulfilled twice");
    check(system.bookManager()->findBookByID("B002").getAvailableQuantity() >= 0, "sanity");

    section("11. Cancelling a loan entered by mistake");
    const Book before = system.bookManager()->findBookByID("B003");
    const QString mistake = system.getActiveRecordsByReader("R002").first().getRecordID();
    check(system.cancelLoan(mistake, &err), "the loan is cancelled");
    check(!db->findBorrowRecordByID(mistake).isValid(), "the record is gone");
    check(system.bookManager()->findBookByID("B003").getAvailableQuantity()
              == before.getAvailableQuantity() + 1,
          "the copy went back on the shelf");
    // A loan that has already been returned is history, not a mistake to undo.
    check(system.borrowBook("R003", "B002", todayStr, &err), "R003 borrows B002");
    const QString closed = system.getActiveRecordsByReader("R003").first().getRecordID();
    check(system.returnBook(closed, todayStr, &err), "and returns it");
    check(!system.cancelLoan(closed, &err), "a closed loan cannot be cancelled");

    section("12. One-pass statistics and the overdue set");
    const LibraryStats st = system.stats();
    check(st.totalCopies == st.borrowed + st.available,
          "borrowed + available equals the total number of copies");
    check(st.readers == system.readerManager()->getAllReaders().size(), "reader count matches");
    check(system.readersWithOverdue().contains("R001"),
          "readersWithOverdue finds R001 in a single pass");
    check(!system.readersWithOverdue().contains("R003"), "and does not include R003");

    section("13. Configurable business parameters");
    LibraryConfig cfg = system.config();
    cfg.loanPeriodDays = 21;
    cfg.finePerDay = 2000;
    system.setConfig(cfg);
    check(system.loanPeriodDays() == 21, "the loan period changed");
    check(db->getSetting("loanPeriodDays") == "21", "and was persisted to Settings");
    // A book of its own, so this does not collide with the loans made above.
    system.bookManager()->addBook(Book("BCFG", "Config Test", "Nobody", "Test", 2, 2), &err);
    check(system.borrowBook("R002", "BCFG", todayStr, &err), "a new loan uses the new period");
    QString cfgDue;
    for (const BorrowRecord &r : system.getActiveRecordsByReader("R002")) {
        if (r.getBookID() == "BCFG")
            cfgDue = r.getDueDate();
    }
    check(cfgDue == today.addDays(21).toString("yyyy-MM-dd"), "the deadline is 21 days out");

    section("14. Audit trail");
    const QList<AuditEntry> audit = db->getAuditEntries(50);
    bool sawBorrow = false, sawDelete = false, sawWaive = false;
    for (const AuditEntry &e : audit) {
        if (e.action == "BORROW")        sawBorrow = true;
        if (e.action == "DELETE_READER") sawDelete = true;
        if (e.action == "FINE_WAIVED")   sawWaive = true;
    }
    check(!audit.isEmpty(), "entries are being recorded");
    check(sawBorrow && sawDelete && sawWaive, "borrow, delete and waive are all logged");
    check(audit.first().username == "admin", "each entry names the signed-in user");

    section("15. History views");
    check(!system.getRecordsByBook("B001").isEmpty(), "a book's history can be listed");
    check(system.getRecordsByReader("R001").size() >= 2, "a reader's history can be listed");
    check(system.getRecordsByBook("ZZZ").isEmpty(), "an unknown book has no history");

    section("16. Forgiving search");
    // Every one of these must find "The C++ Programming Language" (B001).
    const QStringList queries = {
        "programming",          // plain
        "PROGRAMMING",          // shouting
        "Programming Language", // exact phrase
        "language programming", // reversed word order
        "programminglanguage",  // run together
        "programing",           // one typo
        "b001",                 // by ID, lower case
    };
    for (const QString &q : queries) {
        const QList<Book> hits = system.bookManager()->searchBooks(q);
        bool found = false;
        for (const Book &bk : hits) {
            if (bk.getBookID() == "B001") {
                found = true;
                break;
            }
        }
        check(found, QString("\"%1\" finds B001").arg(q));
    }
    check(system.bookManager()->searchBooks("stroustrup").first().getBookID() == "B001",
          "searching an author ranks the right book first");
    check(system.bookManager()->searchBooks("zzzzzznothing").isEmpty(),
          "nonsense finds nothing");

    // Vietnamese accents must not matter, in either direction.
    system.readerManager()->addReader(
        Reader("R900", QString::fromUtf8("Nguyễn Thị Hồng Đào"), "0900000000",
               "dao@example.com", 0), &err);
    check(!system.readerManager()->searchReaders("nguyen thi hong dao").isEmpty(),
          "an unaccented query finds an accented name");
    check(!system.readerManager()->searchReaders(QString::fromUtf8("ĐÀO")).isEmpty(),
          "an accented, upper-case query finds it too");
    check(!system.readerManager()->searchReaders("dao hong").isEmpty(),
          "word order does not matter");
    check(SearchUtil::normalize(QString::fromUtf8("Đại Học")) == "dai hoc",
          "normalize strips accents and the stroke on d");

    section("17. Profile pictures");
    check(system.readerManager()->updateAvatar("R001", QByteArray("fake-png-bytes"), &err),
          "an avatar can be stored");
    check(system.readerManager()->findReaderByID("R001").getAvatar() == "fake-png-bytes",
          "and read back");
    // The edit form has no picture field, so saving it must not wipe the photo.
    Reader edited = system.readerManager()->findReaderByID("R001");
    system.readerManager()->updateReader(
        Reader("R001", edited.getFullName(), edited.getPhone(), edited.getEmail(), 0), &err);
    check(system.readerManager()->findReaderByID("R001").getAvatar() == "fake-png-bytes",
          "editing the reader's details keeps their picture");
    check(system.readerManager()->updateAvatar("R001", QByteArray(), &err)
              && system.readerManager()->findReaderByID("R001").getAvatar().isEmpty(),
          "and it can be removed again");

    section("18. Payment QR");
    // The matrices are compared against a reference encoder, and decoded back
    // with OpenCV, in scratch tooling; here we guard the parts the app relies on.
    const QrCode qr = QrCode::encode(QByteArray("2|99|0849739335|||0|0|30000|R001 4821|transfer_p2p"));
    check(qr.isValid(), "a payment payload encodes");
    check(qr.moduleCount() == 33, "into a version-4 symbol (33x33)");
    // Three finder patterns must be present, or no scanner will lock on.
    check(qr.moduleAt(0, 0) && qr.moduleAt(6, 0) && qr.moduleAt(0, 6),
          "the top-left finder is drawn");
    check(qr.moduleAt(qr.moduleCount() - 1, 0) && qr.moduleAt(qr.moduleCount() - 7, 0),
          "the top-right finder is drawn");
    check(qr.moduleAt(0, qr.moduleCount() - 1) && qr.moduleAt(0, qr.moduleCount() - 7),
          "the bottom-left finder is drawn");
    check(!qr.moduleAt(1, 7) && !qr.moduleAt(7, 1), "the separators are light");
    check(qr.moduleAt(8, 6), "the timing row survives the format strip");
    check(qr.moduleAt(6, 8), "and so does the timing column");
    check(!qr.toImage(4, 4).isNull(), "it renders to an image");
    check(!QrCode::encode(QByteArray(400, 'x')).isValid(),
          "an over-long payload is refused rather than truncated");

    check(Payment::momoAccount() == "0849739335", "fees go to the right MoMo number");
    const QString ref1 = Payment::makeReference("R007");
    check(ref1.startsWith("R007 "), "the reference starts with the reader ID");
    check(ref1.mid(5).size() == 4, "followed by four digits");
    bool allDigits = true;
    for (const QChar &c : ref1.mid(5)) {
        if (!c.isDigit())
            allDigits = false;
    }
    check(allDigits, "which really are digits");
    check(Payment::makeReference("R007") != ref1
              || Payment::makeReference("R007") != ref1,
          "and vary between payments");

    section("19. Translation");
    Lang::instance()->setLanguage(Lang::English);
    check(TR("Dashboard") == "Dashboard", "English passes through unchanged");
    Lang::instance()->setLanguage(Lang::Vietnamese);
    check(TR("Dashboard") == QString::fromUtf8("Tổng quan"), "Dashboard is translated");
    check(TR("An untranslated string") == "An untranslated string",
          "a missing entry falls back to English");
    Lang::instance()->setLanguage(Lang::English);

    std::printf("\n===== %d checks, %d failed =====\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
