// Pins the authored native dialogue contracts. No game data.
#include "../../src/native/gameplay/dialogue.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void testOpenStartsAtFirstLine() {
    Dialogue dialogue;
    dialogue.open({{"Speaker A", "placeholder first"}, {"Speaker B", "placeholder second"}, {"Speaker C", "placeholder third"}});

    CHECK(dialogue.isActive());
    CHECK(dialogue.currentLine() != nullptr);
    CHECK(dialogue.currentLine()->speaker == "Speaker A");
    CHECK(dialogue.currentLine()->text == "placeholder first");
}

static void testConfirmAdvancesAndFinishesAfterLastLine() {
    Dialogue dialogue;
    dialogue.open({{"Speaker A", "placeholder first"}, {"Speaker B", "placeholder second"}, {"Speaker C", "placeholder third"}});

    CHECK(!dialogue.advance({.confirm = true}));
    CHECK(dialogue.currentLine()->text == "placeholder second");
    CHECK(!dialogue.advance({.confirm = true}));
    CHECK(dialogue.currentLine()->text == "placeholder third");
    CHECK(dialogue.advance({.confirm = true}));
    CHECK(!dialogue.isActive());
    CHECK(dialogue.currentLine() == nullptr);
}

static void testCancelClosesImmediately() {
    Dialogue dialogue;
    dialogue.open({{"Speaker", "placeholder"}});

    CHECK(dialogue.advance({.cancel = true}));
    CHECK(!dialogue.isActive());
    CHECK(dialogue.currentLine() == nullptr);
    CHECK(!dialogue.advance({.confirm = true}));
}

static void testCloseClearsCurrentLine() {
    Dialogue dialogue;
    dialogue.open({{"Speaker", "placeholder"}});
    dialogue.close();

    CHECK(!dialogue.isActive());
    CHECK(dialogue.currentLine() == nullptr);
}

static void testBoundedTextTruncatesAtMaxLength() {
    CHECK(boundedText("placeholder", 4) == "plac");
    CHECK(boundedText("placeholder", 32) == "placeholder");
    CHECK(boundedText("placeholder", 0).empty());
}

int main() {
    testOpenStartsAtFirstLine();
    testConfirmAdvancesAndFinishesAfterLastLine();
    testCancelClosesImmediately();
    testCloseClearsCurrentLine();
    testBoundedTextTruncatesAtMaxLength();
    if (g_fail != 0) return EXIT_FAILURE;
    std::puts("dialogue tests passed");
    return EXIT_SUCCESS;
}
