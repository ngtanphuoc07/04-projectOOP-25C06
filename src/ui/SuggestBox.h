#ifndef SUGGESTBOX_H
#define SUGGESTBOX_H

#include <QStringList>
#include <functional>

class QLineEdit;

// Search-engine style suggestions under a search field.
//
// The candidate list is fetched lazily through the supplied callback each time
// the user types, so the suggestions always reflect the current data without
// anybody having to remember to refresh them. Ranking reuses SearchUtil, so a
// suggestion appears for accent-free, reordered, run-together and mistyped
// input exactly as the search itself does.
namespace SuggestBox {

using Source = std::function<QStringList()>;

// Most suggestions to show at once.
int maxSuggestions();

void attach(QLineEdit *field, Source source);

} // namespace SuggestBox

#endif // SUGGESTBOX_H
