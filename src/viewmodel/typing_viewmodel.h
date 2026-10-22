#pragma once
#include "typing_model.h"
#include "subjects.h"
namespace viewmodel {
class TypingViewModel {
public:
    TypingViewModel();
    void key(uint32_t key, uint64_t now);
    void tick(uint64_t now);
    void save();
    const model::TypingModel& state() const { return state_; }
    lv_subject_t* revision_subject() { return revision_.native(); }
    bool quit_requested() const { return quit_; }
private:
    void publish();
    model::TypingModel state_;
    reactive::IntSubject revision_{0};
    uint64_t last_save_{0};
    bool quit_{false};
};
}
