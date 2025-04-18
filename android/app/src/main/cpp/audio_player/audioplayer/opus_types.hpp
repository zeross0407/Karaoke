#pragma once
#include <cstdint>
#include <cstdio>
#include <ogg/ogg.h>

#if defined(__ANDROID__)
    #include <opus.h>
#else
    #include <opus/opus.h>
#endif

struct OpusHeader {
    int version;
    int channels;
    int preskip;
    uint32_t input_sample_rate;
    int gain;
    int channel_mapping;
    int nb_streams;
    int nb_coupled;
    unsigned char stream_map[255];
};

//Dùng để lưu trữ thông tin về các page trong file Ogg
struct OggPageIndex {
    uint16_t index;         // index của page trong page_table
    int64_t granule_pos;    // granule position của page
    int64_t file_offset;    // vị trí byte đầu tiên của page trong file
    uint32_t size;          // kích thước của page (header_len + body_len)
};

//Vị trí bắt đầu của một O
struct OggPageStartPos {
    uint16_t index;      // index của page trong page_table
    int64_t file_offset; //vị trí byte đầu tiên của page trong file
    int64_t granule_pos; //granule position của page trước đó
};

//Dùng để lưu trữ thông tin về file Ogg
struct OggOpusFile {
    FILE *fin;
    
    OpusHeader header;
    ogg_sync_state oy;
    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;
    OpusDecoder *decoder;

    // Index table structures
    std::vector<OggPageIndex> page_table;     // Lưu trữ tuần tự các page index

    // Thông tin về thời lượng và vị trí
    ogg_int64_t last_granulepos;  // Granulepos của page cuối cùng
    uint32_t file_duration;           // Thời lượng (milliseconds)
};