#include "gameplay/dialogue.h"

#include <utility>

namespace rt::native::gameplay {

void Dialogue::open(std::vector<DialogueLine> lines) {
    lines_ = std::move(lines);
    current_ = 0;
}

bool Dialogue::advance(const MenuInput& input) {
    if (!isActive()) return false;
    if (input.cancel) {
        close();
        return true;
    }
    if (!input.confirm) return false;

    ++current_;
    if (current_ == lines_.size()) {
        close();
        return true;
    }
    return false;
}

const DialogueLine* Dialogue::currentLine() const {
    return isActive() ? &lines_[current_] : nullptr;
}

bool Dialogue::isActive() const {
    return current_ < lines_.size();
}

void Dialogue::close() {
    lines_.clear();
    current_ = 0;
}

std::string boundedText(const std::string& text, std::size_t maxLen) {
    return text.substr(0, maxLen);
}

}  // namespace rt::native::gameplay
