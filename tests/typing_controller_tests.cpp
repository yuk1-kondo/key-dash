#include "typing_viewmodel.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
 const auto directory=std::filesystem::temp_directory_path()/("key-dash-controller-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 setenv("KEY_DASH_STATE_DIR",directory.c_str(),1);lv_init();
 try {
  viewmodel::TypingViewModel vm;
  vm.key('1',100);require(vm.state().page()==model::TypingPage::Ready,"menu prepares game");
  vm.key(vm.state().word()[0],1000);require(vm.state().input().size()==1,"physical letter routes");
  vm.key(LV_KEY_BACKSPACE,1100);require(vm.state().input().empty(),"backspace routes");
  const auto theme=vm.state().records().daylight;
  vm.key('0',1200);require(vm.state().input().empty() && vm.state().mistakes()==1 && vm.state().records().daylight==theme,"game input cannot toggle theme");
  vm.key(LV_KEY_ENTER,1250);require(vm.state().input().empty() && vm.state().mistakes()==1,"Enter does not submit or count as a typing attempt");
  const auto word=vm.state().word();
  for (char ch : word) vm.key(ch,1300);
  require(vm.state().completed_words()==1 && vm.state().input().empty(),"controller advances immediately without Enter");
  vm.key(LV_KEY_ESC,31000);
  require(vm.state().page()==model::TypingPage::Results && vm.state().records().rounds==1,"deadline processed before escape");
  require(std::filesystem::exists(directory/"state.txt"),"finished result saved");
  vm.key(LV_KEY_ESC,32000);vm.key(LV_KEY_NEXT,33000);require(vm.state().records().selected_pack==model::WordPack::Code,"tab changes word pack in menu");
  vm.key('4',34000);require(vm.state().page()==model::TypingPage::Help,"help opens");
  vm.key(LV_KEY_ESC,35000);require(vm.state().page()==model::TypingPage::Home,"help closes");
  vm.key(LV_KEY_ESC,36000);require(vm.quit_requested(),"escape exits from menu");
  std::filesystem::remove_all(directory);std::cout<<"PASS: keyboard routing, correction, theme isolation, exact-deadline Escape, saving, menu, help and exit\n";
 }catch(const std::exception& error){std::filesystem::remove_all(directory);std::cerr<<error.what()<<'\n';return 1;}
}
