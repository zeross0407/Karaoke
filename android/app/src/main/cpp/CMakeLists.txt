cmake_minimum_required(VERSION 3.10.2)

# Đặt tên cho project
project("recorder")

# ========================== Thêm thư viện con ==========================
# Biên dịch thư viện Oboe từ source
add_subdirectory(${CMAKE_SOURCE_DIR}/oboe)
add_subdirectory(${CMAKE_SOURCE_DIR}/opus)
add_subdirectory(${CMAKE_SOURCE_DIR}/ogg)
add_subdirectory(${CMAKE_SOURCE_DIR}/rubberband)

# ========================== Biên dịch thư viện native ==========================
# Tạo thư viện native_lib từ file nguồn
add_library(recorder SHARED recorder/main.cpp)

add_library(player SHARED
    audio_player/app/main.cpp
    audio_player/app/ogg_play_advanced.cpp
    audio_player/app/ogg_play_basic.cpp
    audio_player/audioplayer/audio_layer_factory.cpp
    audio_player/audioplayer/audio_player.cpp
    audio_player/audioplayer/audio_session.cpp
    audio_player/audioplayer/audio_session_ogg.cpp
    audio_player/audioplayer/audio_session_ogg_analyze.cpp
    audio_player/audioplayer/audio_session_ogg_seek.cpp
    audio_player/audioplayer/audio_session_ogg_play.cpp
    audio_player/audioplayer/audio_session_resample.cpp
    audio_player/audioplayer/thread_pool.cpp
    audio_player/audioplayer/oboe_layer.cpp
    audio_player/audioplayer/error_code.cpp
    audio_player/audioplayer/ring_buffer.cpp
)


add_library(karaoke SHARED
    karaoke/android_karaoke.cpp
    karaoke/android_mic_player.cpp
    karaoke/karaoke_factory.cpp
    karaoke/export.cpp
)




include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/ogg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/include
    ${CMAKE_CURRENT_SOURCE_DIR}/oboe/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ========================== Cấu hình Include ===========================
# Thêm đường dẫn thư mục chứa file header để biên dịch
# Bao gồm cả các thư viện bên ngoài (Oboe, Opus, Ogg)

target_include_directories(recorder PRIVATE ${CMAKE_SOURCE_DIR}/oboe/include)
target_include_directories(recorder PRIVATE ${CMAKE_SOURCE_DIR}/opus/include)
target_include_directories(recorder PRIVATE ${CMAKE_SOURCE_DIR}/ogg/include)
target_include_directories(recorder PRIVATE ${CMAKE_SOURCE_DIR})

target_include_directories(player PRIVATE ${CMAKE_SOURCE_DIR}/oboe/include)
target_include_directories(player PRIVATE ${CMAKE_SOURCE_DIR}/opus/include)
target_include_directories(player PRIVATE ${CMAKE_SOURCE_DIR}/ogg/include)
target_include_directories(player PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(player PRIVATE ${CMAKE_SOURCE_DIR}/rubberband)
target_include_directories(player PRIVATE ${CMAKE_SOURCE_DIR}/audioplayer/audioplayer)


target_include_directories(karaoke PRIVATE ${CMAKE_SOURCE_DIR}/oboe/include)
target_include_directories(karaoke PRIVATE ${CMAKE_SOURCE_DIR}/opus/include)
target_include_directories(karaoke PRIVATE ${CMAKE_SOURCE_DIR}/ogg/include)
target_include_directories(karaoke PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(karaoke PRIVATE ${CMAKE_SOURCE_DIR}/rubberband)
target_include_directories(karaoke PRIVATE ${CMAKE_SOURCE_DIR}/audioplayer/audioplayer)
target_include_directories(karaoke PRIVATE ${CMAKE_SOURCE_DIR}/audio_player/app)


# ========================== Liên kết thư viện ===========================
# Tìm thư viện log của Android NDK
find_library(log-lib log)

# Liên kết native_lib với các thư viện cần thiết
target_link_libraries(recorder
    oboe   # Thư viện Oboe (biên dịch từ source)
    log    # Thư viện log của Android
    opus   # Thư viện Opus (biên dịch từ source)
    ogg    # Thư viện Ogg (biên dịch từ source)
    OpenSLES
    c++_shared
    atomic
    m)

# ========================== Thêm thư viện audio_player ==========================
target_link_libraries(player
    oboe   # Thư viện Oboe (biên dịch từ source)
    log    # Thư viện log của Android
    opus   # Thư viện Opus (biên dịch từ source)
    ogg   # Thư viện Ogg (biên dịch từ source)
    rubberband
    OpenSLES
    c++_shared
    atomic
    m
)

# ========================== Thêm thư viện karaoke ==========================
target_link_libraries(karaoke
    oboe   # Thư viện Oboe (biên dịch từ source)
    log    # Thư viện log của Android
    opus   # Thư viện Opus (biên dịch từ source)
    ogg   # Thư viện Ogg (biên dịch từ source)
    rubberband
    player # Thêm player vì karaoke sử dụng OggPlay từ player
    OpenSLES
    c++_shared
    atomic
)