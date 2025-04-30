import 'package:flutter/material.dart';
import 'package:highlighpro/ffi/audio_player_ffi.dart';
import 'package:highlighpro/ffi/karaoke_ffi.dart';
import 'package:permission_handler/permission_handler.dart';

main() {
  runApp(const MaterialApp(home: FixTestScreen()));
}

class FixTestScreen extends StatefulWidget {
  const FixTestScreen({Key? key}) : super(key: key);

  @override
  State<FixTestScreen> createState() => _FixTestScreenState();
}

class _FixTestScreenState extends State<FixTestScreen> {
  String _status = 'Chưa khởi tạo';
  bool _isLoading = false;
  final KaraokeFFIFixed _ffi = KaraokeFFIFixed();

  // Danh sách assets
  final List<String> _melodyAssets = [
    'assets/cmbg_back.ogg',
    'assets/bn_back.ogg',
  ];

  final List<String> _lyricAssets = ['assets/cmbg_vo.ogg'];

  // Controllers cho dropdown
  String _selectedMelody = 'assets/bn_back.ogg';
  String _selectedLyric = 'assets/cmbg_vo.ogg';

  @override
  void initState() {
    super.initState();
    CAudioPlayer.initLibrary();
    _requestPermissions();
  }

  @override
  void dispose() {
    super.dispose();
  }

  // Yêu cầu quyền ghi âm
  Future<void> _requestPermissions() async {
    try {
      final status = await Permission.microphone.request();
      setState(() {
        _status = 'Quyền ghi âm: ${status.isGranted ? "Đã cấp" : "Chưa cấp"}';
      });
    } catch (e) {
      setState(() {
        _status = 'Lỗi khi yêu cầu quyền: $e';
      });
    }
  }

  Future<void> _initializeFFI() async {
    if (_isLoading) return;

    // Kiểm tra quyền trước khi khởi tạo
    final permissionStatus = await Permission.microphone.status;
    if (!permissionStatus.isGranted) {
      await _requestPermissions();
      // Nếu vẫn không có quyền sau khi yêu cầu
      if (!(await Permission.microphone.isGranted)) {
        setState(() {
          _status = 'Khởi tạo thất bại: Không có quyền ghi âm';
        });
        return;
      }
    }

    setState(() {
      _isLoading = true;
      _status = 'Đang khởi tạo...';
    });

    try {
      final result = _ffi.initialize();
      setState(() {
        _status = result ? 'Khởi tạo thành công' : 'Khởi tạo thất bại';
      });
    } catch (e) {
      setState(() {
        _status = 'Lỗi: $e';
      });
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  Future<void> _testKaraoke() async {
    //CAudioPlayer.playAssetAudio('assets/cmbg_back.ogg');
    //CAudioPlayer.initLibrary();
    if (_isLoading || !_ffi.isInitialized) return;

    setState(() {
      _isLoading = true;
      _status = 'Đang kiểm tra...';
    });

    try {
      // Dùng method mới hỗ trợ assets
      final result = await _ffi.testKaraokeWithAssets(
        _selectedMelody,
        _selectedMelody,
      );
      setState(() {
        _status = result ? 'Kiểm tra thành công' : 'Kiểm tra thất bại';
      });
    } catch (e) {
      setState(() {
        _status = 'Lỗi: $e';
      });
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  // Hàm tạo dropdown cho melody
  Widget _buildMelodyDropdown() {
    return DropdownButtonFormField<String>(
      decoration: const InputDecoration(
        labelText: 'Chọn file melody',
        border: OutlineInputBorder(),
      ),
      value: _selectedMelody,
      onChanged: _isLoading
          ? null
          : (String? newValue) {
              if (newValue != null) {
                setState(() {
                  _selectedMelody = newValue;
                });
              }
            },
      items: _melodyAssets.map<DropdownMenuItem<String>>((String value) {
        return DropdownMenuItem<String>(
          value: value,
          child: Text(value.split('/').last),
        );
      }).toList(),
    );
  }

  // Hàm tạo dropdown cho lyric
  Widget _buildLyricDropdown() {
    return DropdownButtonFormField<String>(
      decoration: const InputDecoration(
        labelText: 'Chọn file lyric',
        border: OutlineInputBorder(),
      ),
      value: _selectedLyric,
      onChanged: _isLoading
          ? null
          : (String? newValue) {
              if (newValue != null) {
                setState(() {
                  _selectedLyric = newValue;
                });
              }
            },
      items: _lyricAssets.map<DropdownMenuItem<String>>((String value) {
        return DropdownMenuItem<String>(
          value: value,
          child: Text(value.split('/').last),
        );
      }).toList(),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Karaoke Test - Dùng Assets')),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              'Trạng thái: $_status',
              style: const TextStyle(fontSize: 18),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 20),

            // Melody dropdown
            _buildMelodyDropdown(),
            const SizedBox(height: 10),

            // Lyric dropdown
            _buildLyricDropdown(),
            const SizedBox(height: 20),

            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Expanded(
                  child: ElevatedButton(
                    onPressed: _isLoading ? null : _initializeFFI,
                    child: const Text('Khởi tạo FFI'),
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: ElevatedButton(
                    onPressed: (_isLoading || !_ffi.isInitialized)
                        ? null
                        : _testKaraoke,
                    child: const Text('Kiểm tra Karaoke'),
                  ),
                ),
              ],
            ),
            if (_isLoading)
              const Padding(
                padding: EdgeInsets.only(top: 20.0),
                child: Center(child: CircularProgressIndicator()),
              ),
          ],
        ),
      ),
    );
  }
}
