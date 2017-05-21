#include <QMediaPlayer>
#include <QDesktopWidget>
#include <QSharedPointer>
#include <QColor>
#include <QFile>
#include <QThread>
#include <QTimer>
#include <QDir>

#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    player(new QMediaPlayer(this))
{
    ui->setupUi(this);

    initWindow();

    connect(ui->start_button, &QToolButton::clicked,
            this, &MainWindow::start);

    connect(ui->pause_button, &QToolButton::clicked,
            this, &MainWindow::pause);

    connect(ui->progress_slider, &QSlider::sliderPressed,
            [=]() { this->player->pause(); });

    connect(ui->progress_slider, &QSlider::sliderReleased,
            this, &MainWindow::updateProgressBySlider);

    connect(ui->volume_slider, &QSlider::sliderMoved,
            this, &MainWindow::updateVolume);

    connect(player, &QMediaPlayer::positionChanged,
            this, &MainWindow::updateProgressByTick);

    connect(ui->submit_button, &QPushButton::clicked,
            this, &MainWindow::evaluate);

    connect(ui->resource_list, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::selectResource);

    checkResource();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initWindow()
{
    ui->start_button->setStyleSheet("background-color: rgb(255, 0, 0);");

    ui->pause_button->setStyleSheet("background-color: rgb(160, 160, 160);");

    ui->volume_slider->setValue(40);

    QDesktopWidget* desktop = QApplication::desktop();

    move(desktop->width() / 2 - width() / 2,
         desktop->height() / 2 - height() / 2);


    auto directory = QCoreApplication::applicationDirPath();

    player->setVolume(40);
}

void MainWindow::start()
{
    if (player->isMetaDataAvailable())
    {
        player->play();
    }
    else
    {
        ui->statusBar->showMessage("no resource assigned", 2000);
    }
}

void MainWindow::pause()
{
    player->pause();
}

void MainWindow::updateProgressByTick()
{
    if (player->state() == QMediaPlayer::State::PlayingState)
    {
        double progress = 100.0 * player->position() / player->duration();

        ui->progress_slider->setValue(static_cast<int>(progress));

        ui->progress_time_lable->setText(timeString());
    }
}

void MainWindow::updateProgressBySlider()
{
    double rate = ui->progress_slider->value() / 100.0;

    int current = static_cast<int>(rate * player->duration());

    ui->progress_time_lable->setText(timeString());

    player->setPosition(current);

    player->play();
}

void MainWindow::updateVolume()
{
    player->setVolume(ui->volume_slider->value());
}

namespace {

enum class State
{
    KEPT, REMOVED, INSERTED
};

class WordState
{
public:
    WordState(const QString& word, State state)
        : word(word)
        , state(state)
    {
    }

    const QString& getWord() const
    {
        return word;
    }

    State getState() const
    {
        return state;
    }

private:
    QString word;
    State state;
};

using Expense = int;

using Index = int;

using Coordinate = std::pair<Index, Index>;

using Node = std::tuple<Coordinate, Expense, State>;

Coordinate makeCoordinate(int x, int y)
{
    return std::make_pair(x, y);
}

Index getX(const Coordinate& coordinate)
{
    return coordinate.first;
}

Index getY(const Coordinate& coordinate)
{
    return coordinate.second;
}

constexpr Node makeNode(Coordinate coordinate,
                        Expense expense, State state)
{
    return std::make_tuple(coordinate, expense, state);
}

#define define_getter(target, i) \
    auto get ## target (const Node& node) \
{ \
    return std::get<i>(node); \
}

define_getter(NextCoordinate, 0)
define_getter(Expense, 1)
define_getter(State, 2)

#undef define_getter


using Memo = QMap<Coordinate, Node>;

const int REMOVE_EXPENSE = 2;
const int INSERT_EXPENSE = 2;
const int KEEP_EXPENSE = 0;

int search(const QStringList* answerWords,
           const QStringList* scriptWords,
           int answerId, int scriptId,
           Memo* memo)
{
    //    KEPT, REMOVED, INSERTED, REPLACED

    if (answerId >= answerWords->size())
    {
        return (scriptWords->size() - scriptId) * REMOVE_EXPENSE;
    }
    else if (scriptId >= scriptWords->size())
    {
        return (answerWords->size() - answerId) * INSERT_EXPENSE;
    }
    else
    {
        auto iter = memo->find({ answerId, scriptId });

        if (iter == memo->end())
        {
            const auto& answer = answerWords->at(answerId);
            const auto& script = scriptWords->at(scriptId);

            int better = -1;
            Node node;

            if (script == answer)
            {
                better = KEEP_EXPENSE + search(answerWords, scriptWords,
                                               answerId + 1, scriptId + 1,
                                               memo);

                node = makeNode(makeCoordinate(answerId + 1, scriptId + 1),
                                better, State::KEPT);

            }
            else
            {
                int removed = REMOVE_EXPENSE + search(answerWords, scriptWords,
                                                      answerId, scriptId + 1,
                                                      memo);

                int inserted = INSERT_EXPENSE + search(answerWords, scriptWords,
                                                       answerId + 1, scriptId,
                                                       memo);

                if (removed <= inserted)
                {
                    better = removed;

                    node = makeNode(makeCoordinate(answerId, scriptId + 1),
                                    better, State::REMOVED);
                }
                else
                {
                    better = inserted;

                    node = makeNode(makeCoordinate(answerId + 1, scriptId),
                                    better, State::INSERTED);
                }
            }

            memo->insert({ answerId, scriptId }, node);

            return better;
        }
        else
        {
            return getExpense(iter.value());
        }
    }
}

QSharedPointer<QList<WordState>> search(const QStringList* answerWords,
                                        const QStringList* scriptWords)
{
    Memo memo;

    search(answerWords, scriptWords, 0, 0, &memo);

    auto wordStateList = QSharedPointer<QList<WordState>>::create();

    auto current = makeCoordinate(0, 0);
    auto iter = memo.end();

    while ((iter = memo.find(current)) != memo.end())
    {
        auto state = getState(*iter);
        auto next = getNextCoordinate(*iter);

        const QString* word = nullptr;

        if (state == State::KEPT || state == State::INSERTED)
        {
            word = &answerWords->at(getX(current));

            wordStateList->push_back(WordState(*word, state));
        }
        else
        {
            word = &scriptWords->at(getY(current));

            wordStateList->push_back(WordState(*word, state));
        }

        current = next;
    }

    if (getX(current) >= answerWords->size())
    {
        for (int i = getY(current), bound = scriptWords->size();
             i < bound; ++i)
        {
            auto w = WordState(scriptWords->at(i), State::REMOVED);

            wordStateList->push_back(w);
        }
    }
    else if (getY(current) >= scriptWords->size())
    {
        for (int i = getX(current), bound = answerWords->size();
             i < bound; ++i)
        {
            auto w = WordState(answerWords->at(i), State::INSERTED);

            wordStateList->push_back(w);
        }
    }

    return wordStateList;
}

} //! end anonymous namespace

void MainWindow::evaluate()
{
    if (textFile.isEmpty())
    {
        statusBar()->showMessage("resource not assigned", 2000);
        return;
    }

    QString script = ui->script_edit->toPlainText();

    QStringList scriptsWords = script
            .replace(QRegExp("\\W+"), " ").toLower().split(' ');

    QString dir = QCoreApplication::applicationDirPath();

    QFile path(textFile);

    if (!path.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << textFile;
        statusBar()->showMessage("source text not found", 2000);
        return;
    }

    QTextStream is(&path);

    QString answer = is.readAll();

    QStringList asnwerWords = answer
            .replace(QRegExp("\\W+"), " ").toLower().split(' ');

    // here dynamic programming...

    auto list = search(&asnwerWords, &scriptsWords);

    ui->script_edit->clear();


    QTextCharFormat keptFormat;
    keptFormat.setForeground(QBrush(Qt::GlobalColor::black));

    QTextCharFormat insertedFormat;
    insertedFormat.setForeground(QBrush(Qt::GlobalColor::red));

    QTextCharFormat removedFormat;
    removedFormat.setForeground(QBrush(Qt::GlobalColor::blue));

    for (const WordState& s : *list)
    {
        switch (s.getState())
        {
            case State::KEPT:
                ui->script_edit->setCurrentCharFormat(keptFormat);
                break;

            case State::INSERTED:
                ui->script_edit->setCurrentCharFormat(insertedFormat);
                break;

            case State::REMOVED:
                ui->script_edit->setCurrentCharFormat(removedFormat);
                break;

            default:
                break;
        }

        ui->script_edit->insertPlainText(s.getWord());
        ui->script_edit->insertPlainText(" ");
    }
}

enum TreeItemType
{
    UNIT = 1,
    SECTION
};

void MainWindow::checkResource()
{
    QString path = QCoreApplication::applicationDirPath();

    QDir dataDirectory(path + "/english_data");

    if (!dataDirectory.exists())
    {
        qDebug() << "resource not found";
        return;
    }

    QRegExp pattern("(.+)\\.mp3");

    auto sectionDirectories = dataDirectory
            .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (auto& sectionDir : sectionDirectories)
    {
        const auto& sectionPath = QDir(sectionDir.absoluteFilePath());

        auto section = new QTreeWidgetItem(ui->resource_list,
                                           TreeItemType::SECTION);

        section->setText(0, sectionDir.fileName());

        for (const auto& file: sectionPath.entryInfoList(QDir::Files))
        {
            if (pattern.indexIn(file.absoluteFilePath()) != -1)
            {
                auto unit = new QTreeWidgetItem(section, TreeItemType::UNIT);

                unit->setText(0, file.fileName());

                // no effect
                if (!QFile(pattern.capturedTexts().at(1)).exists())
                {
                    unit->setText(1, "source text not fount");
                }
            }
        }
    }
}

void MainWindow::selectResource(QTreeWidgetItem* item, int)
{
    if (item->type() == TreeItemType::UNIT)
    {
        QString workingDirectory = QCoreApplication::applicationDirPath();

        QTreeWidgetItem* section = item->parent();

        QString resourcePath = QString("%1/english_data/%2/%3")
                .arg(workingDirectory)
                .arg(section->text(0))
                .arg(item->text(0));

        player->setMedia(QUrl::fromLocalFile(resourcePath));

        textFile = resourcePath.remove(".mp3");

        statusBar()->showMessage("resource selected", 2000);
    }
}

QString MainWindow::timeString() const
{
    int current = player->position() / 1000;
    int total = player->duration() / 1000;

    return QString("%1:%2/%3:%4")
            .arg(current / 60, 2, 10, QChar('0'))
            .arg(current % 60, 2, 10, QChar('0'))
            .arg(total / 60, 2, 10, QChar('0'))
            .arg(total % 60, 2, 10, QChar('0'));
}

