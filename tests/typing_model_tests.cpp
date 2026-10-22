#include "typing_model.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void complete_word(model::TypingModel& state, uint64_t& time) {
 const auto word = state.word();
 for (char character : word) { state.type(character, time); time += 120; }
 state.type(' ', time); time += 120;
}
int main() {
 const auto directory = std::filesystem::temp_directory_path() / ("key-dash-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 std::filesystem::create_directories(directory);
 try {
  using model::TypingMode; using model::TypingPage;
  model::TypingModel state(directory/"state.txt");
  state.prepare(TypingMode::Sprint30,10);
  require(state.page()==TypingPage::Ready && state.remaining_ms(999999)==30000,"timer waits for first character");
  state.type(' ',100); state.backspace(200);
  require(state.page()==TypingPage::Ready && state.attempts()==0,"empty submits/backspaces do not start timer");
  const auto word=state.word(); const char wrong=word[0]=='x'?'z':'x';
  state.type(wrong,1000); require(state.page()==TypingPage::Running && state.accuracy()==0,"first typo starts clock and counts");
  state.type(' ',1100); require(state.completed_words()==0 && state.attempts()==2,"wrong word cannot be skipped");
  state.backspace(1200); require(state.input().empty() && state.attempts()==2,"backspace corrects without erasing history");
  uint64_t time=1300; complete_word(state,time);
  require(state.completed_words()==1 && state.input().empty(),"exact word plus space advances");
  require(state.word()!=word,"no immediate repeated word");
  const auto correct=static_cast<unsigned>(word.size()+1);
  require(state.accuracy()==correct*100/(correct+2),"corrected typos remain in accuracy");
  require(state.remaining_ms(30999)==1,"exact timer boundary");
  const auto attempts=state.attempts();state.type(state.word()[0],31000);
  require(state.page()==TypingPage::Results && state.attempts()==attempts,"late key cannot score");
  require(state.elapsed_ms(100000)==30000,"late poll never extends round duration");
  require(state.records().rounds==1 && state.records().words==1,"one completed session");
  require(!state.tick(100000) && state.records().rounds==1,"result cannot be awarded twice");
  require(state.save(),"first save succeeds");
  model::TypingModel restored(directory/"state.txt");
  require(restored.records().rounds==1 && restored.records().words==1,"records survive reload");
  model::TypingModel rewrites(directory/"rewrite.txt");rewrites.prepare(TypingMode::Sprint30,0);
  const auto target=rewrites.word();rewrites.type(target[0],1000);rewrites.type(target[1],1500);
  const auto speed=rewrites.wpm(2500);rewrites.backspace(1800);rewrites.type(target[1],2000);
  require(rewrites.wpm(2500)==speed,"rewriting correct letters cannot inflate WPM");
  rewrites.type(target[2]=='x'?'z':'x',2100);
  require(rewrites.correct_prefix()==2,"correct-prefix WPM stops at typo");
  rewrites.back(); require(rewrites.records().rounds==0,"cancelled session changes no records");
  model::TypingModel practice(directory/"practice.txt");practice.prepare(TypingMode::Practice,0);
  time=2000;
  for(unsigned index=0;index<10;++index) complete_word(practice,time);
  require(practice.page()==TypingPage::Results && practice.completed_words()==10,"practice ends after ten confirmed words");
  require(practice.wpm(0)>0 && practice.accuracy()==100 && practice.new_best(),"practice speed and accuracy");
  const auto english_best=practice.best_wpm();practice.back();practice.toggle_pack();
  require(practice.best_wpm()==0,"word packs have separate personal bests");
  practice.toggle_pack();require(practice.best_wpm()==english_best,"English best preserved");
  practice.toggle_theme();require(practice.save(),"settings save");
  model::TypingModel prefs(directory/"practice.txt");
  require(!prefs.records().daylight && prefs.best_wpm()==english_best,"theme and per-mode records restored");
  model::TypingModel uppercase(directory/"upper.txt");uppercase.prepare(TypingMode::Sprint60,0);
  const auto first=uppercase.word()[0];uppercase.type(static_cast<char>(first-'a'+'A'),0);
  require(uppercase.correct_prefix()==1 && uppercase.accuracy()==100,"case insensitive and timestamp zero supported");
  require(!uppercase.tick(59999) && uppercase.tick(60000),"60-second mode boundary");
  {std::ofstream file(directory/"bad.txt");file<<"KEY_DASH 1\n-1 0 0 0 0\n0 0 0 0 0 0\n";}
  model::TypingModel bad(directory/"bad.txt");require(!bad.storage_error().empty(),"invalid save detected");
  bad.toggle_theme();require(!bad.save(),"invalid original never overwritten");
  {std::ifstream file(directory/"bad.txt");std::string content((std::istreambuf_iterator<char>(file)),{});require(content.find("-1")!=std::string::npos,"invalid original preserved");}
  {std::ofstream file(directory/"file");file<<"not a directory";}
  model::TypingModel blocked(directory/"file/state.txt");blocked.toggle_theme();
  require(!blocked.save() && blocked.dirty(),"failed writes retain pending settings");
  std::filesystem::remove_all(directory);
  std::cout<<"PASS: first-key timing, typos, correction history, WPM, deadline, cancellation, practice, independent records, save/reload, corrupt save and write failures\n";
 }catch(const std::exception& error){std::filesystem::remove_all(directory);std::cerr<<error.what()<<'\n';return 1;}
}
