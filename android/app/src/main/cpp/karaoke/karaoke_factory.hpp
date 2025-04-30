#pragma once

#include <memory>
#include <string>

// Forward declaration
class Karaoke;

/**
 * Factory class for creating and managing Karaoke instances
 */
class KaraokeFactory
{
public:
    // Static instance of Karaoke object
    static std::unique_ptr<Karaoke> karaoke;
    static bool is_playing;

    // Creates a new Karaoke instance
    static std::unique_ptr<Karaoke> createKaraoke();

    // Starts a karaoke session
    static void startKaraoke();

    // Stops the current karaoke session
    static void stopKaraoke();

    static void pauseKaraoke();

    static void resumeKaraoke();

    static void setMicVolume(float volume);

    static void setBackgroundVolume(float volume);
};
