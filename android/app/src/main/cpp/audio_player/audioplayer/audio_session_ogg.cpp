#include "audio_session.hpp"
#include "ring_buffer.hpp"
#include <cstring>
#include <chrono>
#include <mutex>

using namespace std;

Result AudioSession::loadFile(const string &fileName)
{
    setState(PlayState::LOADING);
    debugPrint("Loading file: {}", fileName);

    this->fileName = fileName;

    // Mở file
    FILE *fin = fopen(fileName.c_str(), "rb");
    if (!fin)
    {
        setState(PlayState::ERROR);
        return Result::error(ErrorCode::FileNotFound, "Cannot open file");
    }

    // Khởi tạo OggOpusFile
    oggFile = make_unique<OggOpusFile>();
    oggFile->fin = fin;

    // Khởi tạo ogg_sync_state
    ogg_sync_init(&oggFile->oy);

    bool header_parsed = false;
    long page_start_offset = 0;
    uint16_t page_index_counter = 0;

    // Duyệt qua toàn bộ file
    while (true)
    {
        char *buffer = ogg_sync_buffer(&oggFile->oy, OGG_BUFFER_SIZE);
        size_t bytes = fread(buffer, 1, OGG_BUFFER_SIZE, fin);
        if (bytes == 0)
            break;

        ogg_sync_wrote(&oggFile->oy, bytes);

        ogg_page og;
        while (ogg_sync_pageout(&oggFile->oy, &og) == 1)
        {
            if (!header_parsed)
            {
                if (ogg_stream_init(&oggFile->os, ogg_page_serialno(&og)) < 0)
                {
                    setState(PlayState::ERROR);
                    return Result::error(ErrorCode::OggStreamError, "Failed to init ogg stream");
                }

                ogg_stream_pagein(&oggFile->os, &og);

                ogg_packet op;
                if (ogg_stream_packetout(&oggFile->os, &op) != 1)
                {
                    setState(PlayState::ERROR);
                    return Result::error(ErrorCode::OggPacketCorrupt, "Failed to read header packet");
                }

                Result result = parseOpusHeader(&op);
                if (!result.isSuccess())
                {
                    setState(PlayState::ERROR);
                    return result;
                }

                result = initOpusDecoder();
                if (!result.isSuccess())
                {
                    setState(PlayState::ERROR);
                    return result;
                }

                header_parsed = true;
            }

            OggPageIndex page_index;
            page_index.index = page_index_counter++;
            page_index.file_offset = page_start_offset;
            page_index.granule_pos = ogg_page_granulepos(&og);
            page_index.size = og.header_len + og.body_len;

            oggFile->page_table.push_back(page_index);

            if (page_index.granule_pos >= 0)
            {
                oggFile->last_granulepos = page_index.granule_pos;
            }

            page_start_offset += page_index.size;
        }
    }

    if (oggFile->last_granulepos > 0)
    {
        oggFile->file_duration = ((oggFile->last_granulepos - oggFile->header.preskip) * 1000.0) / SAMPLE_RATE;
    }
    else
    {
        setState(PlayState::ERROR);
        return Result::error(ErrorCode::OggMetadataError, "Could not determine duration");
    }
    // Chỉ khi nào load file Opus.ogg thành công thì mới acquireInputBus của AudioLayer
    Result result = acquireInputBus();
    if (!result.isSuccess()) {
        setState(PlayState::ERROR);
        return result;
    }
    // Tạo lại buffer với số kênh đúng
    buffer = make_unique<RingBuffer>(RING_BUFFER_SIZE);
    // Khởi tạo bộ chuyển đổi tần số lấy mẫu (resampler) cho audio session
    initResample();

    fseek(fin, 0, SEEK_SET);
    ogg_sync_reset(&oggFile->oy);
    ogg_stream_reset(&oggFile->os);

    setState(PlayState::READY);
    return Result::success();
}

Result AudioSession::parseOpusHeader(ogg_packet *op)
{
    // Kiểm tra magic signature
    if (op->bytes < 8 || memcmp(op->packet, "OpusHead", 8) != 0)
    {
        return Result::error(ErrorCode::InvalidFormat, "Invalid opus header");
    }

    // Parse header
    OpusHeader *header = &oggFile->header;
    const unsigned char *data = op->packet;

    header->version = data[8];
    header->channels = data[9];
    header->preskip = (data[10] | (data[11] << 8));
    header->input_sample_rate = (data[12] | (data[13] << 8) |
                                 (data[14] << 16) | (data[15] << 24));
    header->gain = (data[16] | (data[17] << 8));
    return Result::success();
}

Result AudioSession::initOpusDecoder()
{
    int error;
    oggFile->decoder = opus_decoder_create(SAMPLE_RATE,
                                           oggFile->header.channels,
                                           &error);

    if (error != OPUS_OK || !oggFile->decoder)
    {
        return Result::error(ErrorCode::DecoderError, "Failed to create decoder");
    }
    return Result::success();
}