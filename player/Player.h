#ifndef PLAYER_H
#define PLAYER_H

class QString;

class Player
{
public:
    Player();

    ~Player();

    bool setMedia(const QString& section, const QString& fileName);

    void play();

    void pause();

    void advance(int milliseconds);

    void reverse(int milliseconds);

    void adjustProgress(double ratio);

    void adjustVolume(double ratio);

private:
    struct Impl;
    Impl* impl;
};

#endif // PLAYER_H
