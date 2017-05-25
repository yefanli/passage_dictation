#ifndef ASSESSOR_H
#define ASSESSOR_H

#include <memory>

class QString;
class Word;

using Path = std::shared_ptr<std::list<Word>>;

Path assess(const QString* source, const QString* input);


#endif // ASSESSOR_H
