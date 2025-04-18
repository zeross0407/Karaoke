cmake_minimum_required(VERSION 3.10.2)
project(rubberband)

# Compile options
set(CMAKE_CXX_STANDARD 14)
add_definitions(
    -DUSE_BUILTIN_FFT 
    -DLACK_POSIX_MEMALIGN 
    -DNDEBUG
    -DUSE_PTHREADS
    -DHAVE_PTHREAD
    # Memory allocation
    -DUSE_OWN_ALIGNED_MALLOC
    -DLACK_POSIX_MEMALIGN
    # System type
    -D__LINUX__
    -DSYSTEM_LINUX
    -DSYSTEM_ANDROID
    # Fix include paths
    -DSYSTEM_INCLUDE_PREFIX="common/"
    # Disable Speex resampler and use built-in resampler
    -DNO_SPEEX
    -DUSE_BQRESAMPLER
)

# Source files - be more explicit about which files to include
set(RUBBERBAND_SOURCES
    src/rubberband-c.cpp
    src/RubberBandStretcher.cpp
    src/RubberBandLiveShifter.cpp
    
    # Common files
    src/common/Allocators.cpp
    src/common/BQResampler.cpp
    src/common/FFT.cpp
    src/common/Log.cpp
    src/common/mathmisc.cpp
    src/common/Profiler.cpp
    src/common/Resampler.cpp
    src/common/StretchCalculator.cpp
    src/common/sysutils.cpp
    src/common/Thread.cpp
    src/common/VectorOpsComplex.cpp
    
    # Faster files
    src/faster/AudioCurveCalculator.cpp
    src/faster/CompoundAudioCurve.cpp
    src/faster/HighFrequencyAudioCurve.cpp
    src/faster/PercussiveAudioCurve.cpp
    src/faster/SilentAudioCurve.cpp
    src/faster/R2Stretcher.cpp
    src/faster/StretcherChannelData.cpp
    src/faster/StretcherProcess.cpp
    
    # Finer files - R3Stretcher is the main implementation
    src/finer/R3Stretcher.cpp
    src/finer/R3LiveShifter.cpp
)

# Check if files exist in ext directory and add them if they do
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/ext/kissfft")
    add_definitions(-DHAVE_KISSFFT)
    set(RUBBERBAND_SOURCES ${RUBBERBAND_SOURCES}
        src/ext/kissfft/kiss_fft.c
        src/ext/kissfft/kiss_fftr.c
    )
endif()

# Create library
add_library(rubberband STATIC ${RUBBERBAND_SOURCES})

# Include directories
target_include_directories(rubberband PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/rubberband
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/faster
    ${CMAKE_CURRENT_SOURCE_DIR}/src/finer
)

# Add ext directories if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/ext/kissfft")
    target_include_directories(rubberband PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ext/kissfft
    )
endif()

# Link libraries
target_link_libraries(rubberband
    PRIVATE
    m
) 