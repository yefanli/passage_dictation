#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <QString>
#include <QList>

class Hunspell;

class SpellChecker
{
public:
    SpellChecker();

    ~SpellChecker();

    bool isValid(const QString& word) const;

private:
    Hunspell* checker;
};

#endif // DICTIONARY_H
