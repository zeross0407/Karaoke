class LyricsData {
  final String text;
  final List<LyricSegment> segments;

  LyricsData({
    required this.text,
    required this.segments,
  });

  factory LyricsData.fromJson(Map<String, dynamic> json) {
    return LyricsData(
      text: json['text'] as String,
      segments: (json['segments'] as List)
          .map((segment) => LyricSegment.fromJson(segment))
          .toList(),
    );
  }
}

class LyricSegment {
  final double start;
  final double end;
  final String text;
  final List<LyricWord> words;

  LyricSegment({
    required this.start,
    required this.end,
    required this.text,
    required this.words,
  });

  factory LyricSegment.fromJson(Map<String, dynamic> json) {
    return LyricSegment(
      start: double.parse(json['start'].toString()),
      end: double.parse(json['end'].toString()),
      text: json['text'] as String,
      words: (json['words'] as List)
          .map((word) => LyricWord.fromJson(word))
          .toList(),
    );
  }
}

class LyricWord {
  final String word;
  final double start;
  final double end;
  final int note;

  LyricWord({
    required this.word,
    required this.start,
    required this.end,
    required this.note,
  });

  factory LyricWord.fromJson(Map<String, dynamic> json) {
    return LyricWord(
      word: json['word'] as String,
      start: double.parse(json['start'].toString()),
      end: double.parse(json['end'].toString()),
      note: json['note'] as int,
    );
  }
} 