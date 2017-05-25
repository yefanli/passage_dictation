#include <QMediaPlayer>
#include <QApplication>
#include <QFileInfo>
#include "Player.h"

struct Player::Impl
{
    QString workingDirectory;
    QMediaPlayer player;
};

Player::Player()
    : impl(new Impl)
{
    impl->workingDirectory = QApplication::applicationDirPath();
}

Player::~Player()
{
    if (impl)
    {
        delete impl;
    }
}

bool Player::setMedia(const QString& section, const QString& fileName)
{
    auto path = QString("%1/%2/%3/%4")
            .arg(impl->workingDirectory)
            .arg("resource")
            .arg(section)
            .arg(fileName);

    if (!QFileInfo(path).exists())
    {
        return false;
    }
    else
    {
        impl->player.setMedia(QUrl::fromLocalFile(path));
        return true;
    }
}

void Player::play()
{
    impl->player.play();
}

void Player::pause()
{
    impl->player.pause();
}

void Player::advance(int milliseconds)
{
    auto position = impl->player.position() + milliseconds;
    impl->player.setPosition(position);
}

void Player::reverse(int milliseconds)
{
    auto position = impl->player.position() - milliseconds;
    impl->player.setPosition(position);
}

void Player::adjustProgress(double ratio)
{
    qint64 position = impl->player.duration() * ratio;
    impl->player.setPosition(position);
}

void Player::adjustVolume(double ratio)
{
    impl->player.setVolume(100 * ratio);
}
