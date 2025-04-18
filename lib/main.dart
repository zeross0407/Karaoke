import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:highlighpro/ffi/fix_test_screen.dart';
import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:just_audio/just_audio.dart';
import 'package:path_provider/path_provider.dart';
import 'models/lyric_model.dart';
import 'widgets/karaoke_line_v3.dart';

main() {
  runApp(const MaterialApp(
    home: FixTestScreen(),
  ));
}
// void main() {
//   // Khóa ứng dụng ở chế độ ngang
//   WidgetsFlutterBinding.ensureInitialized();
//   SystemChrome.setPreferredOrientations([
//     DeviceOrientation.landscapeLeft,
//     DeviceOrientation.landscapeRight,
//   ]);

//   runApp(const MyApp());
// }

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: MaterialApp(
        debugShowCheckedModeBanner: false,
        title: 'Karaoke Highlight Demo',
        theme: ThemeData(
          colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurple),
        ),
        home: const KaraokeScreen(),
      ),
    );
  }
}

class KaraokeScreen extends StatefulWidget {
  const KaraokeScreen({super.key});

  @override
  State<KaraokeScreen> createState() => _KaraokeScreenState();
}

class _KaraokeScreenState extends State<KaraokeScreen> {
  final AudioPlayer _audioPlayer = AudioPlayer();
  final ScrollController _scrollController = ScrollController();
  LyricsData? lyricsData;
  bool isLoading = true;
  double _currentTime = 0.0; // Thời gian hiện tại (giây)
  bool _isPlaying = false;
  double _totalDuration = 0.0; // Tổng thời gian của bài hát (giây)
  Timer? _timer;
  DateTime? _lastUpdateTime;
  bool _showControls = true;
  Timer? _controlsTimer;
  bool _showPitchVisualization = true; // Toggle for pitch visualization

  // Các biến liên quan đến hiển thị lời
  int _activeLineIndex = -1;
  List<GlobalKey> _lineKeys = [];

  @override
  void initState() {
    super.initState();
    _loadResources();
    _setupControlsTimer();
  }

  void _setupControlsTimer() {
    // Tự động ẩn điều khiển sau 3 giây
    _controlsTimer?.cancel();
    _controlsTimer = Timer(const Duration(seconds: 3), () {
      if (mounted && _showControls) {
        setState(() {
          _showControls = false;
        });
      }
    });
  }

  Future<void> _loadResources() async {
    try {
      await _loadLyrics();
      await _loadAudio();

      // Lắng nghe trạng thái phát nhạc
      _audioPlayer.playerStateStream.listen((playerState) {
        final processingState = playerState.processingState;

        // Xử lý sự kiện khi bài hát kết thúc
        if (processingState == ProcessingState.completed) {
          _stopPlayback();
          _seekToPosition(0.0);
        }
      });
    } catch (e) {
      print('Error loading resources: $e');
    }
  }

  Future<void> _loadLyrics() async {
    try {
      // Đọc file JSON từ assets
      //final String response = await rootBundle.loadString('assets/tbl_with_notes.json');
      //final String response = await rootBundle.loadString('assets/cttv.json');
      // final String response = await rootBundle.loadString(
      //   'assets/bn_with_notes.json',
      // );
      final String response = await rootBundle.loadString('assets/cmbg.json');
      final data = await json.decode(response);

      setState(() {
        lyricsData = LyricsData.fromJson(data);

        // Tính toán tổng thời gian
        if (lyricsData != null && lyricsData!.segments.isNotEmpty) {
          final lastSegment = lyricsData!.segments.last;
          _totalDuration = lastSegment.end + 5.0; // Thêm 5 giây vào cuối

          // Khởi tạo danh sách keys cho từng dòng
          _lineKeys = List.generate(
            lyricsData!.segments.length,
            (_) => GlobalKey(),
          );
        }
      });
    } catch (e) {
      print('Error loading lyrics: $e');
    }
  }

  Future<void> _loadAudio() async {
    try {
      // Sao chép file MP3 từ assets vào bộ nhớ tạm để có thể đọc từ đó
      //final bytes = await rootBundle.load('assets/tbl.mp3');
      //final bytes = await rootBundle.load('assets/CTTTV.mp3');
      //final bytes = await rootBundle.load('assets/BN.mp3');
      final bytes = await rootBundle.load('assets/CMBG.mp3');
      final dir = await getTemporaryDirectory();
      final file = File('${dir.path}/song.mp3');
      await file.writeAsBytes(
        bytes.buffer.asUint8List(bytes.offsetInBytes, bytes.lengthInBytes),
      );

      // Tải audio từ bộ nhớ tạm
      await _audioPlayer.setFilePath(file.path);

      // Cập nhật tổng thời gian từ file âm thanh nếu cần
      final audioDuration = _audioPlayer.duration;
      if (audioDuration != null) {
        setState(() {
          // Chỉ cập nhật nếu tổng thời gian từ lyrics không đủ
          if (_totalDuration < audioDuration.inMilliseconds / 1000.0) {
            _totalDuration = audioDuration.inMilliseconds / 1000.0;
          }
          isLoading = false;
        });
      } else {
        setState(() {
          isLoading = false;
        });
      }
    } catch (e) {
      setState(() {
        isLoading = false;
      });
      print('Error loading audio: $e');
    }
  }

  // Bắt đầu phát nhạc và timer
  void _startPlayback() {
    if (!_isPlaying) {
      setState(() {
        _isPlaying = true;
      });

      // Bắt đầu audio tại vị trí hiện tại
      _audioPlayer.seek(Duration(milliseconds: (_currentTime * 1000).toInt()));
      _audioPlayer.play();

      // Bắt đầu timer
      _startTimer();
    }
  }

  // Dừng phát nhạc và timer
  void _stopPlayback() {
    if (_isPlaying) {
      setState(() {
        _isPlaying = false;
      });

      _audioPlayer.pause();
      _stopTimer();
    }
  }

  // Bắt đầu timer
  void _startTimer() {
    _stopTimer(); // Đảm bảo không có timer cũ đang chạy

    // Lưu thời điểm bắt đầu để tính toán chính xác
    _lastUpdateTime = DateTime.now();

    // Timer với tần suất 60 lần mỗi giây (16.7ms)
    _timer = Timer.periodic(const Duration(milliseconds: 16), (timer) {
      final now = DateTime.now();

      if (_lastUpdateTime != null) {
        // Tính toán thời gian đã trôi qua từ lần cập nhật trước
        final elapsedSeconds =
            now.difference(_lastUpdateTime!).inMicroseconds / 1000000.0;

        // Thời gian mới
        final newTime = _currentTime + elapsedSeconds;

        // Kiểm tra xem cần cập nhật dòng active
        _updateActiveLine(newTime);

        setState(() {
          // Cập nhật thời gian hiện tại
          _currentTime = newTime;

          // Kiểm tra kết thúc
          if (_currentTime >= _totalDuration) {
            _currentTime = _totalDuration;
            _stopPlayback();
          }
        });
      }

      // Cập nhật mốc thời gian cho lần sau
      _lastUpdateTime = now;
    });
  }

  // Dừng timer
  void _stopTimer() {
    _timer?.cancel();
    _timer = null;
    _lastUpdateTime = null;
  }

  // Cập nhật dòng active và cuộn đến vị trí đó
  void _updateActiveLine(double time) {
    if (lyricsData == null || lyricsData!.segments.isEmpty) return;

    // Tìm dòng active mới
    int newActiveLine = -1;

    for (int i = 0; i < lyricsData!.segments.length; i++) {
      final segment = lyricsData!.segments[i];

      // Dòng đang active hoặc sắp đến
      if (time >= segment.start && time <= segment.end) {
        newActiveLine = i;
        break;
      }
    }

    // Nếu không tìm thấy dòng active, tìm dòng sắp tới
    if (newActiveLine == -1) {
      for (int i = 0; i < lyricsData!.segments.length; i++) {
        final segment = lyricsData!.segments[i];
        if (time < segment.start) {
          newActiveLine = i > 0 ? i - 1 : 0;
          break;
        }
      }

      // Nếu vẫn không tìm thấy, lấy dòng cuối cùng
      if (newActiveLine == -1 && lyricsData!.segments.isNotEmpty) {
        newActiveLine = lyricsData!.segments.length - 1;
      }
    }

    // Nếu dòng active thay đổi, cập nhật UI và cuộn đến dòng đó
    if (newActiveLine != _activeLineIndex && newActiveLine != -1) {
      setState(() {
        _activeLineIndex = newActiveLine;
      });

      // Cuộn đến dòng active với animation mượt mà
      if (_lineKeys.isNotEmpty && _activeLineIndex < _lineKeys.length) {
        final RenderObject? renderObject =
            _lineKeys[_activeLineIndex].currentContext?.findRenderObject();
        if (renderObject != null && renderObject is RenderBox) {
          final position = renderObject.localToGlobal(Offset.zero);

          // Tính toán vị trí cuộn để dòng active ở giữa màn hình
          final centerPosition = position.dy + renderObject.size.height / 2;
          final screenCenter = MediaQuery.of(context).size.height / 2;
          final scrollTo =
              _scrollController.offset + (centerPosition - screenCenter);
          _scrollController.animateTo(
            scrollTo,
            duration: const Duration(milliseconds: 500),
            curve: Curves.easeInOut,
          );
          //_scrollController.jumpTo(scrollTo);
        }
      }
    }
  }

  @override
  void dispose() {
    _stopTimer();
    _controlsTimer?.cancel();
    _scrollController.dispose();
    _audioPlayer.dispose();
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.portraitUp,
      DeviceOrientation.portraitDown,
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
    super.dispose();
  }

  // Xử lý nút play/pause
  void _togglePlayPause() {
    if (_isPlaying) {
      _stopPlayback();
    } else {
      _startPlayback();
    }
    _showControlsTemporarily();
  }

  // Xử lý reset
  void _resetAnimation() {
    _stopPlayback();
    _seekToPosition(0.0);
    _showControlsTemporarily();
  }

  // Xử lý seek
  void _seekToPosition(double seconds) {
    // Giới hạn thời gian trong khoảng hợp lệ
    final newPosition = seconds.clamp(0.0, _totalDuration);

    // Cập nhật dòng active
    _updateActiveLine(newPosition);

    setState(() {
      _currentTime = newPosition;
    });

    // Nếu đang phát, cập nhật vị trí audio
    if (_isPlaying) {
      _audioPlayer.seek(Duration(milliseconds: (newPosition * 1000).toInt()));
    }

    _showControlsTemporarily();
  }

  // Hiển thị controls tạm thời
  void _showControlsTemporarily() {
    setState(() {
      _showControls = true;
    });
    _setupControlsTimer();
  }

  @override
  Widget build(BuildContext context) {
    if (isLoading) {
      return const Scaffold(
        backgroundColor: Colors.black,
        body: Center(child: CircularProgressIndicator(color: Colors.white)),
      );
    }

    if (lyricsData == null) {
      return const Scaffold(
        backgroundColor: Colors.black,
        body: Center(
          child: Text(
            'Không thể tải lời bài hát',
            style: TextStyle(color: Colors.white, fontSize: 20),
          ),
        ),
      );
    }

    // Calculate the width for the pitch visualization column
    final double pitchColumnWidth = MediaQuery.of(context).size.width * 0.05;

    return GestureDetector(
      onTap: _showControlsTemporarily,
      child: Scaffold(
        backgroundColor: Colors.black,
        body: Stack(
          children: [
            // 0. Container for pitch visualization and lyrics
            Row(
              children: [
                // Pitch visualization column on the left (conditionally shown)

                // Lyrics display (takes full width if pitch viz is hidden)
                Expanded(
                  child: Center(
                    child: Container(
                      //padding: const EdgeInsets.symmetric(horizontal: 24),
                      child: ClipRRect(
                        // Thêm hiệu ứng mờ dần ở trên và dưới danh sách
                        child: ShaderMask(
                          shaderCallback: (Rect rect) {
                            return LinearGradient(
                              begin: Alignment.topCenter,
                              end: Alignment.bottomCenter,
                              colors: [
                                Colors.transparent,
                                Colors.black,
                                Colors.black,
                                Colors.transparent,
                              ],
                              stops: const [0.0, 0.1, 0.9, 1.0],
                            ).createShader(rect);
                          },
                          blendMode: BlendMode.dstIn,
                          child: ListView.builder(
                            controller: _scrollController,
                            padding: EdgeInsets.symmetric(
                              vertical:
                                  MediaQuery.of(context).size.height * 0.4,
                            ),
                            itemCount: lyricsData!.segments.length,
                            itemBuilder: (context, index) {
                              final segment = lyricsData!.segments[index];
                              final isActive = index == _activeLineIndex;

                              return Container(
                                key: _lineKeys[index],
                                margin: const EdgeInsets.symmetric(
                                  vertical: 20.0,
                                ),
                                child: KaraokeLineV3(
                                  segment: segment,
                                  currentTime: _currentTime,
                                  fontSize: isActive ? 32 : 26,
                                  isCurrentLine: isActive,
                                ),
                              );
                            },
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
              ],
            ),

            // 2. Thanh điều khiển - hiển thị khi _showControls = true
            Positioned(
              left: 0,
              right: 0,
              bottom: 0,
              child: AnimatedOpacity(
                opacity: _showControls ? 1.0 : 0.0,
                duration: const Duration(milliseconds: 300),
                child: Container(
                  color: Colors.black.withOpacity(0.7),
                  padding: const EdgeInsets.symmetric(
                    vertical: 12,
                    horizontal: 16,
                  ),
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      // Main controls row
                      Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          // Thời gian hiện tại
                          Text(
                            _formatDuration(_currentTime),
                            style: const TextStyle(color: Colors.white),
                          ),

                          const SizedBox(width: 8),

                          // Nút điều khiển phát/dừng
                          IconButton(
                            padding: EdgeInsets.zero,
                            onPressed: _togglePlayPause,
                            icon: Icon(
                              _isPlaying
                                  ? Icons.pause_circle_filled
                                  : Icons.play_circle_filled,
                              color: Colors.white,
                              size: 40,
                            ),
                          ),

                          // Thanh tiến trình
                          Expanded(
                            child: SliderTheme(
                              data: SliderTheme.of(context).copyWith(
                                thumbShape: const RoundSliderThumbShape(
                                  enabledThumbRadius: 8,
                                ),
                                trackHeight: 4.0,
                                trackShape: const RoundedRectSliderTrackShape(),
                                activeTrackColor: Colors.deepPurple,
                                inactiveTrackColor: Colors.grey[700],
                                thumbColor: Colors.white,
                                overlayColor: Colors.deepPurple.withOpacity(
                                  0.4,
                                ),
                              ),
                              child: Slider(
                                value: _currentTime.clamp(0.0, _totalDuration),
                                min: 0.0,
                                max: _totalDuration,
                                onChanged: (value) {
                                  _seekToPosition(value);
                                },
                              ),
                            ),
                          ),

                          // Tổng thời gian
                          Text(
                            _formatDuration(_totalDuration),
                            style: const TextStyle(color: Colors.white),
                          ),

                          const SizedBox(width: 8),

                          // Nút reset
                          IconButton(
                            padding: EdgeInsets.zero,
                            onPressed: _resetAnimation,
                            icon: const Icon(
                              Icons.replay,
                              color: Colors.white,
                              size: 30,
                            ),
                          ),

                          // Toggle pitch visualization button
                          IconButton(
                            padding: EdgeInsets.zero,
                            onPressed: () {
                              setState(() {
                                _showPitchVisualization =
                                    !_showPitchVisualization;
                              });
                            },
                            icon: Icon(
                              _showPitchVisualization
                                  ? Icons.analytics_outlined
                                  : Icons.analytics,
                              color:
                                  _showPitchVisualization
                                      ? Colors.deepPurple
                                      : Colors.white,
                              size: 24,
                            ),
                            tooltip: 'Hiển thị biểu đồ pitch',
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  // Định dạng thời gian thành mm:ss
  String _formatDuration(double seconds) {
    int totalSeconds = seconds.round();
    int min = totalSeconds ~/ 60;
    int sec = totalSeconds % 60;
    return '$min:${sec.toString().padLeft(2, '0')}';
  }
}
