#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QMediaPlayer;
class QTreeWidgetItem;

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
    void start();
    void pause();

    void updateProgressByTick();

    void updateProgressBySlider();

    void updateVolume();

    void evaluate();

    void selectResource(QTreeWidgetItem* item, int column);

private:
    void initWindow();

    void checkResource();

    QString timeString() const;

    double timeRate() const;

private:
    Ui::MainWindow *ui;
    QMediaPlayer* player;
    QString textFile;
};

#endif // MAINWINDOW_H
