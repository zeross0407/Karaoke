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
    
    // Creates a new Karaoke instance
    static std::unique_ptr<Karaoke> createKaraoke();
    
    // Starts a karaoke session with melody and lyric files
    static void startKaraoke(const char* melody, const char* lyric);
    
    // Stops the current karaoke session
    static void stopKaraoke();
};
