#pragma once
#include <string>
#include <string_view>

enum class ErrorCode {
    NoError = 0,
    Unknown,
    InvalidParameter,
    MemoryAllocFailed,
    Timeout,
    FileNotFound,
    FileReadError,
    InvalidFormat,
    UnsupportedFormat,
    DecoderError,
    
    // AudioLayer errors
    AudioSetupError,

    
    // Ogg specific errors
    OggSyncError,
    OggStreamError,
    OggInvalidFormat,
    OggPacketCorrupt,
    OggMetadataError,

    // Opus specific errors
    OpusInvalidHeader,
    OpusHeaderVersionUnsupported,
    OpusDecodeError,
    
    ResampleError,
    // Buffer errors
    BufferFull,
    EndOfFile,

    // State errors
    InvalidState,
    NotInitialized,
    NotReady,
    
    // Playback errors  
    PlaybackError,
    SeekError,
    LoopError,
    
    // Buffer operation errors
    BufferOverflow,
    BufferUnderflow,
    BufferInvalidOperation,

    // WAV specific errors
    MaxSessionsReached,
};

// Add global function declaration
std::string_view getErrorString(ErrorCode code) noexcept;

struct Result {
    ErrorCode code;
    std::string message;

    Result() : code(ErrorCode::NoError) {}
    Result(ErrorCode c, std::string msg = "") : code(c), message(std::move(msg)) {}

    [[nodiscard]] bool isSuccess() const {
        return code == ErrorCode::NoError;
    }

    [[nodiscard]] static Result success() {
        return Result();
    }

    [[nodiscard]] static Result error(ErrorCode code, std::string message = "") {
        return {code, message.empty() ? std::string(::getErrorString(code)) : std::move(message)};
    }

    // Utility methods
    [[nodiscard]] std::string_view getErrorString() const noexcept {
        return ::getErrorString(code);
    }
};