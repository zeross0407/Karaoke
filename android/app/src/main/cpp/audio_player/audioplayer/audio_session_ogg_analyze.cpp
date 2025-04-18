#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include "audio_session.hpp"
#include "error_code.hpp"

using namespace std;
/**
 * Duyệt tuần tự và in thông tin chi tiết của tất cả ogg_page trong file Opus/Ogg
 * 
 * @param filename Đường dẫn đến file Opus/Ogg
 * @return 0 nếu thành công, mã lỗi nếu thất bại
 */
Result AudioSession::printOggPageInfo(const string &fileName) {
    FILE* file = fopen(fileName.c_str(), "rb");
    if (!file) {
        return Result::error(ErrorCode::FileNotFound, "Cannot open file " + fileName);
    }
    
    // Khởi tạo Ogg sync state
    ogg_sync_state oy;
    ogg_sync_init(&oy);
    
    // Biến đếm và theo dõi
    int page_count = 0;
    long file_offset = 0;
    ogg_int64_t total_samples = 0;

        
    // In tiêu đề
    cout << "┌─────────┬────────────┬────────────┬──────────┬──────────┬──────────┬────────────┬───────────────┬───────────────┐\n";
    cout << "│ Page #  │File Offset │ Page Size  │ Header   │ Body     │ Packets  │ Serial #   │ Granulepos    │ Time (ms)     │\n";
    cout << "├─────────┼────────────┼────────────┼──────────┼──────────┼──────────┼────────────┼───────────────┼───────────────┤\n";
    
    // Duyệt qua file
    while (true) {
        // Lưu vị trí hiện tại trong file
        file_offset = ftell(file);
        
        // Đọc dữ liệu vào buffer
        char* buffer = ogg_sync_buffer(&oy, 4096);
        size_t bytes = fread(buffer, 1, 4096, file);
        if (bytes == 0) break; // Hết file
        ogg_sync_wrote(&oy, bytes);
        
        // Tìm và xử lý các page
        ogg_page og;
        while (ogg_sync_pageout(&oy, &og) == 1) {
            page_count++;
            
            // Lấy thông tin cơ bản của page
            int header_len = og.header_len;
            int body_len = og.body_len;
            int total_len = header_len + body_len;
            int serial_no = ogg_page_serialno(&og);
            ogg_int64_t granulepos = ogg_page_granulepos(&og);
            
            // Tính số packet trong page
            int packet_count = 0;
            if (header_len > 27) { // Kiểm tra header có đủ dài không
                int num_segments = og.header[26]; // Số lượng segment trong page
                int current_packet_size = 0;
                
                // Duyệt qua các segment length trong header
                for (int i = 0; i < num_segments && (27 + i) < header_len; i++) {
                    int segment_length = og.header[27 + i];
                    current_packet_size += segment_length;
                    
                    // Nếu segment length < 255, đây là segment cuối của packet
                    if (segment_length < 255) {
                        packet_count++;
                        current_packet_size = 0;
                    }
                }
            }
            
            // Tính thời gian dựa trên granulepos
            double time_ms = -1.0;
            if (granulepos >= 0) {
                time_ms = (double)granulepos * 1000.0 / SAMPLE_RATE;
            }
            
            // In thông tin page
            cout << "│ " << setw(7) << page_count
                      << " │ " << setw(10) << file_offset
                      << " │ " << setw(10) << total_len
                      << " │ " << setw(8) << header_len
                      << " │ " << setw(8) << body_len
                      << " │ " << setw(8) << packet_count
                      << " │ " << setw(8) << serial_no
                      << " │ " << setw(13) << granulepos
                      << " │ " << setw(13) << (time_ms >= 0 ? to_string((int)time_ms) : "N/A")
                      << " │\n";
            
            // Kiểm tra các cờ (flags) của page
            if (ogg_page_bos(&og)) {
                cout << "│         │ [Beginning of Stream]                                                                       │\n";
            }
            if (ogg_page_eos(&og)) {
                cout << "│         │ [End of Stream]                                                                             │\n";
            }
            if (ogg_page_continued(&og)) {
                cout << "│         │ [Continued packet from previous page]                                                       │\n";
            }
            
            // Cập nhật vị trí file
            file_offset += total_len;
        }
    }
    
    // In footer
    cout << "└─────────┴────────────┴────────────┴──────────┴──────────┴──────────┴────────────┴───────────────┴───────────────┘\n";
    
    // In tổng kết
    cout << "Total pages: " << page_count << endl;
    
    // Giải phóng tài nguyên
    ogg_sync_clear(&oy);
    fclose(file);
    
    return Result::success();
}

/**
 * Phân tích chi tiết hơn về các packet trong mỗi page
 * 
 * @param filename Đường dẫn đến file Opus/Ogg
 * @param detailed_packets In thông tin chi tiết về từng packet
 * @return 0 nếu thành công, mã lỗi nếu thất bại
 */
Result AudioSession::analyzeOggFile(const string &fileName, bool detailed_packets) {
    FILE* file = fopen(fileName.c_str(), "rb");
    if (!file) {
        return Result::error(ErrorCode::FileNotFound, "Cannot open file " + fileName);
    }
    
    // Khởi tạo Ogg sync và stream state
    ogg_sync_state oy;
    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;
    
    ogg_sync_init(&oy);
    bool stream_initialized = false;
    
    // Biến đếm và theo dõi
    int page_count = 0;
    int packet_count = 0;
    int SAMPLE_RATE = 48000; // Giá trị mặc định cho Opus
    
    cout << "Analyzing Ogg/Opus file: " << fileName << endl;
    cout << "----------------------------------------\n";
    
    // Duyệt qua file
    while (true) {
        // Đọc dữ liệu vào buffer
        char* buffer = ogg_sync_buffer(&oy, 4096);
        size_t bytes = fread(buffer, 1, 4096, file);
        if (bytes == 0) break; // Hết file
        ogg_sync_wrote(&oy, bytes);
        
        // Tìm và xử lý các page
        while (ogg_sync_pageout(&oy, &og) == 1) {
            page_count++;
            
            // Khởi tạo stream state nếu đây là page đầu tiên
            if (!stream_initialized) {
                ogg_stream_init(&os, ogg_page_serialno(&og));
                stream_initialized = true;
            }
            
            // Kiểm tra nếu page thuộc về stream khác
            if (ogg_page_serialno(&og) != os.serialno) {
                ogg_stream_reset_serialno(&os, ogg_page_serialno(&og));
            }
            
            // Đưa page vào stream
            ogg_stream_pagein(&os, &og);
            
            // Lấy thông tin page
            ogg_int64_t granulepos = ogg_page_granulepos(&og);
            double time_ms = (granulepos >= 0) ? ((double)granulepos * 1000.0 / SAMPLE_RATE) : -1.0;
            
            cout << "Page " << page_count << ": "
                      << "Granulepos=" << granulepos
                      << ", Time=" << (time_ms >= 0 ? to_string((int)time_ms) + " ms" : "N/A")
                      << ", Size=" << (og.header_len + og.body_len) << " bytes";
            
            if (ogg_page_bos(&og)) cout << " [BOS]";
            if (ogg_page_eos(&og)) cout << " [EOS]";
            if (ogg_page_continued(&og)) cout << " [continued]";
            
            cout << endl;
            
            // Xử lý các packet trong page
            if (detailed_packets) {
                int page_packet_count = 0;
                while (ogg_stream_packetout(&os, &op) == 1) {
                    packet_count++;
                    page_packet_count++;
                    
                    // Tính số mẫu trong packet (chỉ áp dụng cho Opus)
                    int frame_size = -1;
                    if (op.bytes > 0) {
                        frame_size = opus_packet_get_nb_samples(op.packet, op.bytes, SAMPLE_RATE);
                    }
   
                    cout << "  Packet " << packet_count 
                                  << " (Page " << page_count << ", #" << page_packet_count << "): "
                                  << "Size=" << op.bytes << " bytes";
                    
                    
                    
                    if (frame_size > 0) {
                        cout << ", Samples=" << frame_size
                                  << ", Duration=" << (frame_size * 1000.0 / SAMPLE_RATE) << " ms";
                    }
                    
                    if (op.b_o_s) cout << " [BOS]";
                    if (op.e_o_s) cout << " [EOS]";
                    
                    cout << endl;
                }
                
                cout << "  Total packets in page: " << page_packet_count << endl;
            }
        }
    }
    
    cout << "----------------------------------------\n";
    cout << "Summary:\n";
    cout << "  Total pages: " << page_count << endl;
    cout << "  Total packets: " << packet_count << endl;
    
    // Giải phóng tài nguyên
    if (stream_initialized) {
        ogg_stream_clear(&os);
    }
    ogg_sync_clear(&oy);
    fclose(file);
    
    return Result::success();
}

void AudioSession::printOggInfo() const
{
    if (state != PlayState::READY || !oggFile)
    {
        debugPrint("Cannot print Ogg info: File not loaded or not in READY state");
        return;
    }

    printDebug("========== Ogg File Information ==========");
    printDebug("File name: {}", fileName);
    printDebug("Duration: {} ms", this->oggFile->file_duration);

    const OpusHeader &header = oggFile->header;
    printDebug("Version: {}", header.version);
    printDebug("Channels: {}", header.channels);
    printDebug("Pre-skip: {}", header.preskip);
    printDebug("Input sample rate: {} Hz", header.input_sample_rate);
    printDebug("Output gain: {} dB", static_cast<float>(header.gain) / 256.0f);
    
    // In thông tin page table
    printDebug("┌─────────┬────────────┬────────────┬───────────────┐");
    printDebug("│ Page #  │File Offset │ Page Size  │ Granule Pos   │");
    printDebug("├─────────┼────────────┼────────────┼───────────────┤");
    
    for (size_t i = 0; i < oggFile->page_table.size(); i++) {
        const auto& page = oggFile->page_table[i];
        printDebug("│ {:7d} │ {:10d} │ {:10d} │ {:13d} │",
            i,
            page.file_offset,
            page.size,
            page.granule_pos);
    }
    
    printDebug("└─────────┴────────────┴────────────┴───────────────┘");
    printDebug("Total pages: {}\n", oggFile->page_table.size());
}