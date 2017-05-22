#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QMediaPlayer;
class QTreeWidgetItem;
class QWebEngineView;
class SpellChecker;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:    
    // 播放音频
    void start(); 

    // 暂停播放
    void pause();

    // 根据播放进度更新进度条
    void updateProgressByTick();

    // 用户拨动进度条之后更新音频位置
    void updateProgressBySlider();

    // 调节音量
    void updateVolume();

    // 评估用户提交听的写内容
    void evaluate();
    
    // 根据用户的的选择更选音频
    void selectResource(QTreeWidgetItem* item, int column);

    void translateWord();

    void popResourceMenu(QPoint position);

private:
    // 初始化窗口的部分属性
    void initWindow();

    // 搜索工作目录中的资源文件
    void checkResource();

    void showInformation(QString name);

    // 把音频进度转换为时间字符串
    QString timeString() const;

private:
    Ui::MainWindow *ui;
    QMediaPlayer* player;
    QWebEngineView* webView;
    QMenu* resourceMenu;
    // 正在播放的音频对应的原文
    QString textFile;
};

#endif // MAINWINDOW_H
