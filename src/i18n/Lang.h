#ifndef LANG_H
#define LANG_H

#include <QObject>
#include <QString>

// Runtime translation for the whole application.
//
// The English text is also the lookup key, so a string that has no Vietnamese
// entry yet simply stays English instead of showing a raw key like
// "books.title". Wrap every user-visible string in TR(...).
//
// Changing the language emits languageChanged(); MainWindow listens and
// rebuilds its screens, which is simpler and safer than asking every widget to
// re-apply its own texts (a single missed widget would stay in the old
// language forever).
class Lang : public QObject
{
    Q_OBJECT

public:
    enum Language { English = 0, Vietnamese = 1 };

    static Lang *instance();

    Language language() const;
    void setLanguage(Language language);

    // Translate one string. Falls back to the English original.
    static QString text(const QString &english);

signals:
    void languageChanged();

private:
    Lang() = default;

    Language lang = English;
    static Lang *s_instance;
};

// Shorthand used everywhere: TR("Add book")
inline QString TR(const QString &english) { return Lang::text(english); }

#endif // LANG_H
