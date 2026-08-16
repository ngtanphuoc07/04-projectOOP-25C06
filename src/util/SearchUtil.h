#ifndef SEARCHUTIL_H
#define SEARCHUTIL_H

#include <QString>
#include <QStringList>

// Forgiving, search-engine style matching.
//
// A query matches regardless of accents, case, word order, spacing or small
// typos, so all of these find "Lập trình hướng đối tượng":
//
//     lap trinh          (no accents)
//     TRINH lap          (reversed, shouting)
//     laptrinh           (run together)
//     lap trlnh          (one typo)
//
// score() returns 0 for no match and a larger number the better the match, so
// callers can rank results instead of only filtering them.
namespace SearchUtil {

// Lower-cases, strips Vietnamese accents (đ -> d) and collapses whitespace.
QString normalize(const QString &text);

// normalize() with every space removed — used for run-together queries.
QString squash(const QString &text);

// Splits a query into search terms.
QStringList terms(const QString &query);

// 0 = no match. Higher is better:
//   1000  the whole query is a prefix of the field
//    800  the whole query appears somewhere in the field
//    600  every term appears, in any order
//    400  the run-together query appears
//    200  every term matches a word within one typo
int score(const QString &field, const QString &query);

// Best score across several fields, so one call covers "title, author or ID".
int scoreAny(const QStringList &fields, const QString &query);

// Convenience wrapper for callers that only need yes/no.
bool matches(const QStringList &fields, const QString &query);

} // namespace SearchUtil

#endif // SEARCHUTIL_H
