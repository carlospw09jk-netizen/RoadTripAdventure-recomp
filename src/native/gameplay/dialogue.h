#pragma once
// Authored dialogue state model for town/NPC text flow. It stores caller-owned
// authored lines only; rendering and guest-memory access stay outside this API.

#include "gameplay/menu.h"

#include <cstddef>
#include <string>
#include <vector>

namespace rt::native::gameplay {

struct DialogueLine {
    std::string speaker;
    std::string text;
};

class Dialogue {
public:
    void open(std::vector<DialogueLine> lines);
    // Returns true when input completes or cancels an active dialogue.
    bool advance(const MenuInput& input);
    [[nodiscard]] const DialogueLine* currentLine() const;
    [[nodiscard]] bool isActive() const;
    void close();

private:
    std::vector<DialogueLine> lines_;
    std::size_t current_ = 0;
};

// CopyDialogueTextBounded 0x24E7D8-style byte-bounded text copy. std::string
// provides the trailing NUL; byte sequences, including two-byte escapes, pass
// through unchanged up to maxLen.
[[nodiscard]] std::string boundedText(const std::string& text, std::size_t maxLen);

}  // namespace rt::native::gameplay
