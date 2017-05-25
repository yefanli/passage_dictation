#include <map>
#include <vector>
#include <list>
#include <QRegularExpression>
#include "Word.h"
#include "Assessor.h"

using Index = int;

struct Coordinate
{
    Index answerIndex;
    Index inputIndex;
};

bool operator < (const Coordinate& lhs, const Coordinate& rhs)
{
    return std::tie(lhs.answerIndex, lhs.inputIndex)
            < std::tie(rhs.answerIndex, rhs.inputIndex);
}

using Expense = int;

struct Node
{
    Coordinate next;
    Expense expense;
    WordAction action;
};

using Memo = std::map<Coordinate, Node>;

Expense SKIP_SOURCE_EXPENSE = 0;
Expense SKIP_INPUT_EXPENSE = 0;
Expense KEEP_EXPENSE = 0;
Expense REMOVE_EXPENSE = 1;
Expense INSERT_EXPENSE = 1;

int search(const std::vector<QString>* sourceWords,
           const std::vector<QString>* inputWords,
           Index sourceIndex, Index inputIndex,
           Memo* memo)
{
    if (sourceIndex >= (int)sourceWords->size())
    {
        return (inputWords->size() - inputIndex) * REMOVE_EXPENSE;
    }
    else if (inputIndex >= (int)inputWords->size())
    {
        return (sourceWords->size() - sourceIndex) * INSERT_EXPENSE;
    }
    else
    {
        auto iter = memo->find({ sourceIndex, inputIndex });

        if (iter == memo->end())
        {
            const auto& source = sourceWords->at(sourceIndex);
            const auto& input = inputWords->at(inputIndex);

            auto searchNext = [&](Index nextSourceIndex, Index nextInputIndex)
            {
                return search(sourceWords, inputWords,
                              nextSourceIndex, nextInputIndex, memo);
            };

            Node node {};

            if (input == source)
            {
                Expense kept = KEEP_EXPENSE +
                        searchNext(sourceIndex + 1, inputIndex + 1);

                node = Node { { sourceIndex + 1, inputIndex + 1 },
                        kept, WordAction::KEPT };
            }
            else
            {
                if (!source.at(0).isLetterOrNumber())
                {
                    node = Node { { sourceIndex + 1, inputIndex },
                            SKIP_SOURCE_EXPENSE, WordAction::SKIP_SOURCE };
                }
                else if (!input.at(0).isLetterOrNumber())
                {
                    node = Node { { sourceIndex, inputIndex + 1 },
                            SKIP_INPUT_EXPENSE, WordAction::SKIP_INPUT };
                }
                else
                {
                    Expense removed = REMOVE_EXPENSE +
                            searchNext(sourceIndex, inputIndex + 1);

                    Expense inserted = INSERT_EXPENSE
                            + searchNext(sourceIndex + 1, inputIndex);

                    if (removed <= inserted)
                    {
                        node = Node { { sourceIndex, inputIndex + 1 },
                                removed, WordAction::REMOVED };
                    }
                    else
                    {
                        node = Node { { sourceIndex + 1, inputIndex },
                                inserted, WordAction::INSERTED };
                    }
                }
            }

            memo->insert({ { sourceIndex, inputIndex }, node });

            return node.expense;
        }
        else
        {
            return iter->second.expense;
        }
    }
}

// 把 memo 中的路线提取出来
using Path = std::shared_ptr<std::list<Word>>;

Path search(const QString* source, const QString* input)
{
    const auto& ss = source->split(QRegExp("\\b"));
    const auto& is = input->split(QRegExp("\\b"));

    std::vector<QString> sourceWords(ss.cend(), ss.cend());
    std::vector<QString> inputWords(is.cbegin(), is.cend());

    Memo memo;

    ::search(&sourceWords, &inputWords, 0, 0, &memo);

    auto result = std::make_shared<std::list<Word>>();

    auto iter = memo.end();

    Coordinate current { 0, 0 };

    while ((iter = memo.find(current)) != memo.end())
    {
        auto action = iter->second.action;

        if (action == WordAction::KEPT
                || action == WordAction::INSERTED
                || action == WordAction::SKIP_SOURCE)
        {
            result->push_back(Word(source->at(current.answerIndex), action));
        }
        else
        {
            result->push_back(Word(input->at(current.inputIndex), action));
        }

        current = iter->second.next;
    }

    if (current.answerIndex >= source->size())
    {
        for (int i = current.inputIndex, bound = input->size();
             i < bound; ++i)
        {
            result->push_back(Word(input->at(i), WordAction::REMOVED));
        }
    }
    else if (current.inputIndex >= input->size())
    {
        for (int i = current.answerIndex, bound = source->size();
             i < bound; ++i)
        {
            result->push_back(Word(source->at(i), WordAction::INSERTED));
        }
    }

    return result;
}
