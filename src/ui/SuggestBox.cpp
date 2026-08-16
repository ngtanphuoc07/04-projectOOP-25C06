#include "ui/SuggestBox.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QLineEdit>
#include <QStringListModel>

#include <algorithm>

#include "util/SearchUtil.h"

namespace {
constexpr int kMaxSuggestions = 8;
}

namespace SuggestBox {

int maxSuggestions() { return kMaxSuggestions; }

void attach(QLineEdit *field, Source source)
{
    auto *model = new QStringListModel(field);
    auto *completer = new QCompleter(model, field);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer->setMaxVisibleItems(kMaxSuggestions);
    // The list is already ranked and filtered by us, so QCompleter must not
    // filter it again with its own prefix rule.
    completer->setFilterMode(Qt::MatchContains);
    field->setCompleter(completer);

    QObject::connect(field, &QLineEdit::textEdited, field,
                     [field, model, completer, source](const QString &typed) {
        const QString query = typed.trimmed();
        if (query.isEmpty()) {
            model->setStringList({});
            completer->popup()->hide();
            return;
        }

        // Score every candidate, drop duplicates and the ones that do not match,
        // then show the best few.
        QList<QPair<int, QString>> scored;
        QStringList seen;
        const QStringList candidates = source();
        for (const QString &candidate : candidates) {
            const QString text = candidate.trimmed();
            if (text.isEmpty() || seen.contains(text, Qt::CaseInsensitive))
                continue;
            const int s = SearchUtil::score(text, query);
            if (s <= 0)
                continue;
            seen << text;
            scored.append({s, text});
        }

        std::stable_sort(scored.begin(), scored.end(),
                         [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                             return a.first > b.first;
                         });

        QStringList best;
        for (const auto &pair : scored) {
            if (best.size() >= kMaxSuggestions)
                break;
            best << pair.second;
        }

        model->setStringList(best);
        if (best.isEmpty())
            completer->popup()->hide();
        else
            completer->complete();
    });
}

} // namespace SuggestBox
