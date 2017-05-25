#include <QMediaPlayer>
#include <QDesktopWidget>
#include <QSharedPointer>
#include <QWebEngineView>
#include <QColor>
#include <QFile>
#include <QThread>
#include <QTimer>
#include <QDir>
#include <QMimeData>
#include <QTextBlock>
#include <QSyntaxHighlighter>
#include <QMenu>

#include <hunspell/hunspell.h>

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Assessor/Assessor.h"

namespace
{

class WordHighlighter : QSyntaxHighlighter
{
public:
    WordHighlighter(QWidget* parent)
        : QSyntaxHighlighter(parent)
        , handle(nullptr)
        , pattern("\\w+")
    {
        QString workingDirectory = QCoreApplication::applicationDirPath();

        QString aff = workingDirectory + "/en_US/en_US.aff";
        QString dic = workingDirectory + "/en_US/en_US.dic";

        handle = Hunspell_create(aff.toLocal8Bit(), dic.toLocal8Bit());

        right.setForeground(Qt::black);
        error.setForeground(Qt::red);
    }

    ~WordHighlighter()
    {
        if (handle)
        {
            Hunspell_destroy(handle);
        }
    }

protected:
    void highlightBlock(const QString& text) override
    {
        if (!handle)
        {
            setFormat(0, text.length(), right);
            return;
        }

        int index = 0;
        int length = 0;

        while ((index = text.indexOf(pattern, index + length)) != -1)
        {
            length = pattern.matchedLength();

            if (Hunspell_spell(handle, text.mid(index, length).toLocal8Bit()))
            {
                setFormat(index, length, right);
            }
            else
            {
                setFormat(index, length, error);
            }
        }
    }

private:
    Hunhandle* handle;
    QRegExp pattern;
    QTextCharFormat right;
    QTextCharFormat error;
};

} //! end anonymous namespace


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    player(new QMediaPlayer(this)),
    resourceMenu(new QMenu(this))
{
    ui->setupUi(this);

    webView = new QWebEngineView(ui->search_frame);

    new WordHighlighter(ui->script_edit);

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

    connect(ui->tabWidget, &QTabWidget::currentChanged,
            [this](int i) { if (i == 2) this->translateWord(); });

    connect(ui->resource_list, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::popResourceMenu);

    checkResource();

    ui->tabWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initWindow()
{
    // 设置“开始”和“暂停”按钮的的颜色
    ui->start_button->setStyleSheet("background-color: rgb(255, 0, 0);");

    ui->pause_button->setStyleSheet("background-color: rgb(160, 160, 160);");

    ui->volume_slider->setValue(40);

    ui->resource_list->setContextMenuPolicy(Qt::CustomContextMenu);

    // 窗口重定位到桌面中央
    QDesktopWidget* desktop = QApplication::desktop();

    move(desktop->width() / 2 - width() / 2,
         desktop->height() / 2 - height() / 2);

    player->setVolume(40);

    webView->setZoomFactor(0.9);

    webView->show();
}   

void MainWindow::start()
{
    // 如果用户还没有指定要播放的音频， status bar报错
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

// 根据音频进度更新进度条
void MainWindow::updateProgressByTick()
{
    if (player->state() == QMediaPlayer::State::PlayingState)
    {
        double progress = 100.0 * player->position() / player->duration();

        ui->progress_slider->setValue(static_cast<int>(progress));

        ui->progress_time_lable->setText(timeString());
    }
}

// 根据进度条更新音频进度
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

// 匿名空间里的东西全部都是为了 评估 用户提交的听写内容
// 核心： 求最短编辑距离（动态规划）， 返回编辑方法 
namespace
{
// 标识每个单词的状态
enum class State
{
    KEPT, REMOVED, INSERTED
};

class WordAction
{
public:
    WordAction(const QString& word, State state)
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

// 递归求最短距离， 并把编辑路线记在 memo 里面
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

// 把 memo 中的路线提取出来
QSharedPointer<QList<WordAction>> search(const QStringList* answerWords,
                                        const QStringList* scriptWords)
{
    Memo memo;

    search(answerWords, scriptWords, 0, 0, &memo);

    auto wordStateList = QSharedPointer<QList<WordAction>>::create();

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

            wordStateList->push_back(WordAction(*word, state));
        }
        else
        {
            word = &scriptWords->at(getY(current));

            wordStateList->push_back(WordAction(*word, state));
        }

        current = next;
    }

    if (getX(current) >= answerWords->size())
    {
        for (int i = getY(current), bound = scriptWords->size();
             i < bound; ++i)
        {
            auto w = WordAction(scriptWords->at(i), State::REMOVED);

            wordStateList->push_back(w);
        }
    }
    else if (getY(current) >= scriptWords->size())
    {
        for (int i = getX(current), bound = answerWords->size();
             i < bound; ++i)
        {
            auto w = WordAction(answerWords->at(i), State::INSERTED);

            wordStateList->push_back(w);
        }
    }

    return wordStateList;
}

} //! end anonymous namespace

void MainWindow::evaluate()
{
    // 用户还没有指定音频
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

    // 有音频， 但是没有原文
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

    // 下面是把编辑路线输出到QTextEdit里面
    // 正确的单词为黑色
    // 遗漏的单词为红色
    // 多余的单词为蓝色
    QTextCharFormat keptFormat;
    keptFormat.setForeground(QBrush(Qt::GlobalColor::black));

    QTextCharFormat insertedFormat;
    insertedFormat.setForeground(QBrush(Qt::GlobalColor::red));

    QTextCharFormat removedFormat;
    removedFormat.setForeground(QBrush(Qt::GlobalColor::blue));

    for (const WordAction& s : *list)
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

void MainWindow::translateWord()
{
    static QString base("http://www.bing.com/dict/search?q=");

    QTextCursor cursor = ui->script_edit->textCursor();

    if (cursor.hasSelection())
    {
        QString word = cursor.selectedText();

        statusBar()->showMessage("translating ...", 2000);

        webView->load(base + word);
    }
    else if (!webView->url().isValid())
    {
        webView->load(base + "hello");
    }
}

void MainWindow::popResourceMenu(QPoint position)
{
    QTreeWidgetItem* item = ui->resource_list->itemAt(position);

    if (item && item->type() == TreeItemType::UNIT)
    {
        resourceMenu->clear();

        resourceMenu->addAction("play", [this, item]()
        {
           this->selectResource(item, 0);
           this->ui->tabWidget->setCurrentIndex(1);
        });

        resourceMenu->addSeparator();

        resourceMenu->addAction("information", [this, item]
        {
            this->ui->tabWidget->setCurrentIndex(3);
            this->showInformation(item->text(0));
        });

        resourceMenu->addSeparator();

        resourceMenu->addMenu("delete")->addAction("yes", [item]
        {
            item->parent()->removeChild(item);
        });

        resourceMenu->exec(QCursor::pos());
    }
}

void MainWindow::showInformation(QString name)
{

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

