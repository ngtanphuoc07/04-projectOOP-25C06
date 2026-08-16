#include "util/SearchUtil.h"

#include <QRegularExpression>

namespace SearchUtil {

QString normalize(const QString &text)
{
    // Decompose so accents become separate combining marks, then drop them.
    // "Nguyễn" -> "nguyen", "ĐẠI" -> "dai".
    QString out = text.normalized(QString::NormalizationForm_D).toLower();

    QString stripped;
    stripped.reserve(out.size());
    for (const QChar &c : out) {
        if (c.category() == QChar::Mark_NonSpacing)
            continue;                       // an accent that was split off
        if (c == QChar(0x0111))             // đ has no decomposition of its own
            stripped.append('d');
        else
            stripped.append(c);
    }

    static const QRegularExpression spaces("\\s+");
    return stripped.simplified().replace(spaces, " ");
}

QString squash(const QString &text)
{
    return normalize(text).remove(' ');
}

QStringList terms(const QString &query)
{
    const QString norm = normalize(query);
    if (norm.isEmpty())
        return {};
    return norm.split(' ', Qt::SkipEmptyParts);
}

// Levenshtein distance, capped: once it cannot beat 'limit' we stop caring.
static int editDistance(const QString &a, const QString &b, int limit)
{
    if (qAbs(a.size() - b.size()) > limit)
        return limit + 1;

    QList<int> prev(b.size() + 1);
    QList<int> curr(b.size() + 1);
    for (int j = 0; j <= b.size(); ++j)
        prev[j] = j;

    for (int i = 1; i <= a.size(); ++i) {
        curr[0] = i;
        int best = curr[0];
        for (int j = 1; j <= b.size(); ++j) {
            const int cost = (a.at(i - 1) == b.at(j - 1)) ? 0 : 1;
            curr[j] = qMin(qMin(curr[j - 1] + 1, prev[j] + 1), prev[j - 1] + cost);
            best = qMin(best, curr[j]);
        }
        if (best > limit)
            return limit + 1;   // no cell on this row can still win
        prev = curr;
    }
    return prev[b.size()];
}

// How many typos to forgive: none in very short words, where a single edit
// would turn one real word into a different real one.
static int allowedTypos(const QString &term)
{
    if (term.size() <= 3)
        return 0;
    if (term.size() <= 6)
        return 1;
    return 2;
}

int score(const QString &field, const QString &query)
{
    const QString haystack = normalize(field);
    const QString needle = normalize(query);
    if (needle.isEmpty())
        return 1;               // an empty query matches everything
    if (haystack.isEmpty())
        return 0;

    if (haystack.startsWith(needle))
        return 1000;
    if (haystack.contains(needle))
        return 800;

    const QStringList wanted = terms(query);

    // Every term present somewhere, in any order: "trinh lap" finds "lap trinh".
    bool allPresent = !wanted.isEmpty();
    for (const QString &t : wanted) {
        if (!haystack.contains(t)) {
            allPresent = false;
            break;
        }
    }
    if (allPresent)
        return 600;

    // Run-together query: "cleancode" finds "Clean Code".
    if (squash(field).contains(squash(query)))
        return 400;

    // Last resort: allow small typos, term by term against each word.
    const QStringList words = haystack.split(' ', Qt::SkipEmptyParts);
    bool allFuzzy = !wanted.isEmpty();
    for (const QString &t : wanted) {
        const int limit = allowedTypos(t);
        if (limit == 0) {
            allFuzzy = false;
            break;
        }
        bool hit = false;
        for (const QString &w : words) {
            if (editDistance(t, w, limit) <= limit) {
                hit = true;
                break;
            }
            // Also allow a typo inside a longer word: "algoritm" vs "algorithms".
            if (w.size() > t.size() && editDistance(t, w.left(t.size() + limit), limit) <= limit) {
                hit = true;
                break;
            }
        }
        if (!hit) {
            allFuzzy = false;
            break;
        }
    }
    return allFuzzy ? 200 : 0;
}

int scoreAny(const QStringList &fields, const QString &query)
{
    int best = 0;
    for (const QString &f : fields)
        best = qMax(best, score(f, query));
    return best;
}

bool matches(const QStringList &fields, const QString &query)
{
    return scoreAny(fields, query) > 0;
}

} // namespace SearchUtil
