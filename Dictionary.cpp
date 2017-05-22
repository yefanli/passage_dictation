#include "Dictionary.h"
#include <QCoreApplication>
#include <QDebug>

#include <hunspell/hunspell.hxx>

SpellChecker::SpellChecker()
    : checker(nullptr)
{
    QString workingDirectory = QCoreApplication::applicationDirPath();

    QString aff = workingDirectory + "/en_US/en_US.aff";
    QString dic = workingDirectory + "/en_US/en_US.dic";

    if (!QFile(aff).exists())
    {
        qDebug() << "aff file not found";
    }

    if (!QFile(dic).exists())
    {
        qDebug() << "dict file not found";
    }

    checker = new Hunspell(aff.toLocal8Bit(), dic.toLocal8Bit());

    if (!checker)
    {
        qDebug() << "hunspell construction failed";
    }
}

SpellChecker::~SpellChecker()
{
    if (checker)
    {
        delete checker;
    }
}

bool SpellChecker::isValid(const QString& word) const
{
    if (checker)
    {
        return checker->spell(word.toLocal8Bit());
    }
    else
    {
        qDebug() << "hunspell is null";

        return false;
    }
}
