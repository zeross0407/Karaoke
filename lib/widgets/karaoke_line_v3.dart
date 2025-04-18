import 'package:flutter/material.dart';
import '../models/lyric_model.dart';
import 'dart:math';

class KaraokeLineV3 extends StatelessWidget {
  final LyricSegment segment;
  final double currentTime;
  final double fontSize;
  final bool isCurrentLine;

  const KaraokeLineV3({
    super.key,
    required this.segment,
    required this.currentTime,
    required this.fontSize,
    required this.isCurrentLine,
  });

  @override
  Widget build(BuildContext context) {
    // Colors
    final Color normalColor =
        isCurrentLine ? Colors.white : Colors.grey.withOpacity(0.8);
    final Color highlightColor =
        isCurrentLine ? Colors.orangeAccent : Colors.white;
    final Color melodyColor =
        isCurrentLine ? Colors.deepPurple : Colors.grey.withOpacity(0.5);

    // Create rows of KaraokeWords
    return Container(
      width: MediaQuery.of(context).size.width * 0.95, // Tăng độ rộng tối đa
      padding: const EdgeInsets.symmetric(horizontal: 5),
      child: Wrap(
        alignment: WrapAlignment.center,
        spacing: 1, // Giảm spacing xuống còn 1
        runSpacing: 8,
        children: _buildKaraokeWords(normalColor, highlightColor, melodyColor),
      ),
    );
  }

  List<Widget> _buildKaraokeWords(
    Color normalColor,
    Color highlightColor,
    Color melodyColor,
  ) {
    // Tìm min và max thực tế trong segment hiện tại để điều chỉnh line
    // Khởi tạo với giá trị đảm bảo sẽ bị thay thế
    int minNote = 100;
    int maxNote = 20;
    bool hasValidNotes = false;

    // Trước tiên, tìm min và max thực tế (bỏ qua giá trị -1)
    for (final word in segment.words) {
      if (word.note != -1) {
        minNote = minNote > word.note ? word.note : minNote;
        maxNote = maxNote < word.note ? word.note : maxNote;
        hasValidNotes = true;
      }
    }

    // Nếu không tìm thấy note hợp lệ, dùng giá trị mặc định
    if (!hasValidNotes || minNote >= maxNote) {
      minNote = 50;
      maxNote = 70;
    }

    // Tính khoảng cách tối thiểu giữa min và max để đảm bảo sự khác biệt rõ ràng
    final int minRange = 20; // Khoảng cách tối thiểu giữa min và max
    if (maxNote - minNote < minRange) {
      // Nếu khoảng cách quá nhỏ, mở rộng ra
      final int mid = (maxNote + minNote) ~/ 2;
      minNote = mid - (minRange ~/ 2);
      maxNote = mid + (minRange ~/ 2);
    }

    return segment.words.map((word) {
      // Calculate highlight progress
      double wordProgress = 0.0;
      if (currentTime >= word.end) {
        wordProgress = 1.0;
      } else if (currentTime <= word.start) {
        wordProgress = 0.0;
      } else {
        wordProgress = (currentTime - word.start) / (word.end - word.start);
      }

      // Calculate relative pitch position (0.0 - 1.0) based on actual min/max
      double? notePosition;

      if (word.note != -1) {
        // Công thức đơn giản: vị trí = (note - min) / (max - min)
        // Đảm bảo rằng note min sẽ ở vị trí 0.0 (thấp nhất)
        // Và note max sẽ ở vị trí 1.0 (cao nhất)
        notePosition = (word.note - minNote) / (maxNote - minNote);

        // Giới hạn trong khoảng an toàn để tránh lỗi hiển thị
        notePosition = notePosition.clamp(0.0, 1.0);
      }

      return _EnhancedKaraokeWord(
        word: word.word,
        progress: wordProgress,
        notePosition: notePosition,
        fontSize: fontSize,
        normalColor: normalColor,
        highlightColor: highlightColor,
        melodyColor: melodyColor,
        lineWidth: isCurrentLine ? 2.0 : 1.0,
      );
    }).toList();
  }
}

class _EnhancedKaraokeWord extends StatelessWidget {
  final String word;
  final double progress;
  final double? notePosition;
  final double fontSize;
  final Color normalColor;
  final Color highlightColor;
  final Color melodyColor;
  final double lineWidth;

  const _EnhancedKaraokeWord({
    required this.word,
    required this.progress,
    this.notePosition,
    required this.fontSize,
    required this.normalColor,
    required this.highlightColor,
    required this.melodyColor,
    required this.lineWidth,
  });

  @override
  Widget build(BuildContext context) {
    // Đầu tiên render text để đo chính xác kích thước thực tế
    final TextStyle baseStyle = TextStyle(
      fontSize: fontSize,
      fontWeight: FontWeight.bold,
      letterSpacing: -0.3, // Giảm khoảng cách giữa các ký tự thêm
    );

    // Xử lý từ có dấu cách ở đầu - chỉ để tính toán độ rộng thực tế
    String trimmedWord = word;
    String leadingSpace = "";

    // Tách dấu cách đầu để tính toán độ rộng chính xác
    if (word.startsWith(" ")) {
      leadingSpace = " ";
      trimmedWord = word.trimLeft();
    }

    // Tạo TextPainter để đo độ rộng chính xác của text đã loại bỏ dấu cách đầu
    final TextPainter textPainter = TextPainter(
      text: TextSpan(text: trimmedWord, style: baseStyle),
      textDirection: TextDirection.ltr,
    )..layout();

    // Độ rộng thực tế của từ (loại bỏ dấu cách đầu)
    final double textWidth = textPainter.width;

    return Container(
      margin: EdgeInsets.symmetric(horizontal: 1), // Margin nhỏ giữa các từ
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          // Line indicator trên từ (chỉ hiển thị khi có note VÀ đang là dòng hiện tại)
          if (notePosition != null && lineWidth > 1.0)
            Container(
              height:
                  60, // Tăng chiều cao lên gấp đôi so với trước (từ 30 lên 60)
              width:
                  textWidth, // Độ rộng chính xác bằng độ rộng từ đã loại bỏ dấu cách
              margin: EdgeInsets.only(bottom: 2), // Dồn sát vào chữ hơn
              child: CustomPaint(
                size: Size(textWidth, 60),
                painter: _LinePainter(
                  notePosition: notePosition!,
                  color: melodyColor,
                  lineWidth: lineWidth,
                  progress: progress,
                ),
              ),
            )
          else
            SizedBox(height: 62), // Tăng chiều cao tương ứng
          // Hiển thị dấu cách (nếu có) + từ với hiệu ứng highlight
          Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              // Text với hiệu ứng highlight
              _buildText(baseStyle),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildText(TextStyle baseStyle) {
    // No progress, no highlight
    if (progress <= 0) {
      return Text(word, style: baseStyle.copyWith(color: normalColor));
    }

    // Full progress, full highlight
    if (progress >= 1.0) {
      return Text(word, style: baseStyle.copyWith(color: highlightColor));
    }

    // Partial progress, gradient highlight
    return ShaderMask(
      blendMode: BlendMode.srcIn,
      shaderCallback: (bounds) {
        return LinearGradient(
          begin: Alignment.centerLeft,
          end: Alignment.centerRight,
          stops: [
            // Tạo transition mượt hơn giữa highlight và normal
            progress - 0.05 > 0 ? progress - 0.05 : 0,
            progress,
            progress + 0.05 < 1 ? progress + 0.05 : 1,
          ],
          colors: [highlightColor, highlightColor, normalColor],
        ).createShader(Rect.fromLTWH(0, 0, bounds.width, bounds.height));
      },
      child: Text(word, style: baseStyle.copyWith(color: Colors.white)),
    );
  }
}

// Đổi tên _NotePainter thành _LinePainter và đơn giản hóa chỉ còn line không có dot
class _LinePainter extends CustomPainter {
  final double notePosition; // 0.0 (lowest) to 1.0 (highest)
  final Color color;
  final double lineWidth;
  final double progress;

  _LinePainter({
    required this.notePosition,
    required this.color,
    required this.lineWidth,
    required this.progress,
  });

  @override
  void paint(Canvas canvas, Size size) {
    // Sử dụng notePosition trực tiếp, vì đã được tính toán chính xác ở _buildKaraokeWords
    // Để thay đổi hướng: 0.0 = thấp nhất (dưới), 1.0 = cao nhất (trên)
    final double positionFromBottom =
        1.0 - notePosition; // Đảo ngược để vẽ từ dưới lên

    // Tính vị trí y trong container
    final double y = size.height * positionFromBottom;

    // Sử dụng màu cơ bản cho line - không thay đổi màu theo độ cao
    Color lineColor = color;

    // Điều chỉnh độ sáng dựa vào progress (chỉ có 2 trạng thái: mờ và rõ)
    if (progress < 1.0) {
      // Chưa hát xong - màu mờ
      final double opacity =
          0.3 + (0.7 * progress); // Tăng dần độ đậm từ 0.3 đến 1.0 theo tiến độ
      lineColor = lineColor.withOpacity(opacity);
    } else {
      // Đã hát xong - màu rõ ràng
      lineColor = lineColor.withOpacity(1.0);
    }

    // Tạo paint với độ đậm cố định
    final Paint paint =
        Paint()
          ..color = lineColor
          ..strokeWidth =
              lineWidth *
              3 // Giữ độ đậm của line
          ..style = PaintingStyle.stroke
          ..strokeCap = StrokeCap.round;

    // Vẽ đường line theo vị trí đã tính toán
    canvas.drawLine(Offset(0, y), Offset(size.width, y), paint);
  }

  @override
  bool shouldRepaint(covariant _LinePainter oldDelegate) {
    return oldDelegate.notePosition != notePosition ||
        oldDelegate.color != color ||
        oldDelegate.lineWidth != lineWidth ||
        oldDelegate.progress != progress;
  }
}
