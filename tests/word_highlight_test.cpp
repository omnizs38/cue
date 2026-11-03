#include "../src/core/word_highlight.h"
#include <cassert>
#include <iostream>

int main() {
  const std::string input = R"({"segments":[{"words":[{"word":"Hello", "start":1, "end":1.4},{"word":" world.","start":1.4,"end":2}]}]})";
  auto ass = word_highlight_ass(input);
  assert(ass.find("Style: Glow") != std::string::npos);
  assert(ass.find("Hello world.") != std::string::npos);
  assert(ass.find("\\blur6") != std::string::npos);
  assert(ass.find("\\t(0,80,\\3a&H70&)") != std::string::npos);
  assert(ass.find("\\t(400,480,\\3a&H70&)") != std::string::npos);
  assert(ass.find("0:00:01.00,0:00:02.08") != std::string::npos);
  auto reduced = word_highlight_ass(input, true);
  assert(reduced.find("\\t(400,400,\\3a&H70&)") != std::string::npos);
  assert(reduced.find("0:00:01.00,0:00:02.00") != std::string::npos);
  for (auto invalid : {"", "{}", "[]", "{", R"({"words":[]})",
       R"({"words":[{"word":"x","start":-1,"end":1}]})",
       R"({"words":[{"word":"x","start":"1","end":2}]})",
       R"({"words":[{"word":"x","start":1,"end":90000}]})",
       R"({"words":[{"word":" ","start":0,"end":1}]})",
       R"({"words":[{"word":"x","start":1,"end":2},{"word":"y","start":1.9,"end":3}]})",
       R"({"segments":[{"text":"line-timed only","start":0,"end":2}]})"})
    assert(word_highlight_ass(invalid).empty());
  assert(word_highlight_ass(std::string(kWordHighlightMaxInput + 1, ' ')).empty());
  auto escaped = word_highlight_ass(R"({"words":[{"word":"{\\pos(0,0)}\\N","start":0,"end":1}]})");
  assert(!escaped.empty());
  assert(escaped.find("\\pos(") == std::string::npos);
  assert(escaped.find("\\N") == std::string::npos);
  auto spaced = word_highlight_ass(R"({"words":[{"word":"Hello","start":0,"end":1},{"word":"world","start":1,"end":2}]})");
  assert(spaced.find("Hello world") != std::string::npos);
  auto unicode = word_highlight_ass(R"({"words":[{"word":"你好","start":0,"end":1},{"word":"世界","start":1,"end":2}]})");
  assert(unicode.find("你好世界") != std::string::npos);
  auto pause = word_highlight_ass(R"({"words":[{"word":"One","start":0,"end":1},{"word":" two","start":4.123,"end":5}]})");
  assert(pause.find("0:00:00.00,0:00:01.08") != std::string::npos);
  assert(pause.find("\\t(3,83,") != std::string::npos);
  std::string many = "{\"words\":[";
  for (int i = 0; i < 11; ++i) { if (i) many += ','; many += "{\"word\":\" w\",\"start\":" + std::to_string(i) + ",\"end\":" + std::to_string(i+1) + "}"; }
  many += "]}";
  auto split = word_highlight_ass(many);
  assert(split.find("0:00:00.00,0:00:10.00") != std::string::npos);
  assert(split.find("0:00:10.00,0:00:11.08") != std::string::npos);
  auto punctuation = word_highlight_ass(R"({"words":[{"word":"Hi","start":0,"end":1},{"word":"!","start":1,"end":1}]})");
  assert(punctuation.find("Hi!") != std::string::npos);
  std::cout << "word_highlight_test: passed\n";
}
