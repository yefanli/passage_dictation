#ifndef WORD_H
#define WORD_H

#include <QString>

#include "WordAction.h"

class Word
{
public:
    Word(const QString& content, WordAction state)
        : content(content)
        , state(state)
    {
    }

    const QString& getContent() const
    {
        return content;
    }

    WordAction getState() const
    {
        return state;
    }

private:
    QString content;
    WordAction state;
};

#endif // WORD_H
