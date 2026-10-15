// rt-asset-menu: render the authored top-level menu as a screen-space PNG.
#include "gameplay/menu.h"
#include "preview/preview.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace rt::native;

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "usage: rt-asset-menu [output.png]\n";
        return 2;
    }
    const std::string outputPath = argc == 2 ? argv[1] : "menu.png";
    gameplay::MenuFlow flow;
    if (gameplay::tickMenuFlow(flow, {.confirm = true}, 0.0f).type != gameplay::MenuEventType::PAGE_CHANGED ||
        flow.screen != gameplay::MenuScreen::MODE_SELECT) {
        std::cerr << "failed to enter mode select\n";
        return 1;
    }
    gameplay::MenuModel menu{{
        {static_cast<int>(gameplay::MenuMode::ADVENTURE), "Adventure", true},
        {static_cast<int>(gameplay::MenuMode::QUICK_RACE), "Quick Race", true},
        {static_cast<int>(gameplay::MenuMode::TWO_PLAYER), "2 Player", true},
        {static_cast<int>(gameplay::MenuMode::OPTIONS), "Options", true},
    }, 4, 1, 4, flow.topLevelCursor};

    constexpr std::uint32_t kWidth = 1024, kHeight = 720;
    std::string error, device;
    preview::Renderer* renderer = preview::previewInit(kWidth, kHeight, error, &device);
    if (!renderer) {
        std::cerr << "render init failed: " << error << "\n";
        return 1;
    }

    using Quad = preview::ScreenQuad;
    const std::array<float, 4> panelColor{0.08f, 0.12f, 0.22f, 1.0f};
    const std::array<float, 4> rowColor{0.22f, 0.32f, 0.52f, 1.0f};
    const std::array<float, 4> selectedRowColor{0.36f, 0.56f, 0.86f, 1.0f};
    const std::array<float, 4> cursorColor{1.0f, 0.72f, 0.16f, 1.0f};
    std::vector<Quad> quads{
        {112, 72, 800, 576, panelColor},
        {202, 120, 620, 34, {0.55f, 0.78f, 1.0f, 1.0f}},  // title placeholder
    };
    constexpr float rowX = 234, rowY = 204, rowWidth = 556, rowHeight = 58, rowGap = 27;
    for (std::size_t row = 0; row < menu.rows; ++row) {
        const float y = rowY + static_cast<float>(row) * (rowHeight + rowGap);
        if (row == menu.cursorRow)
            quads.push_back({198, y - 8, 628, rowHeight + 16, cursorColor});
        quads.push_back({rowX, y, rowWidth, rowHeight,
                         row == menu.cursorRow ? selectedRowColor : rowColor});
        quads.push_back({rowX + 28, y + 19, rowWidth - 56, 20,
                         {0.88f, 0.93f, 1.0f, 1.0f}});  // label placeholder
    }

    std::vector<std::uint8_t> rgba;
    const bool rendered = preview::previewRender2D(renderer, quads, {0.025f, 0.035f, 0.075f, 1.0f}, rgba, error);
    preview::previewDestroy(renderer);
    if (!rendered) {
        std::cerr << "render failed: " << error << "\n";
        return 1;
    }
    const std::vector<std::uint8_t> png = preview::encodePng(kWidth, kHeight, rgba);
    std::ofstream output(outputPath, std::ios::binary);
    output.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    if (!output) {
        std::cerr << "write failed: " << outputPath << "\n";
        return 1;
    }
    std::cout << "device=" << device << " screen=MODE_SELECT rows=" << menu.rows
              << " cursor_row=" << menu.cursorRow << " output=" << outputPath << "\n";
    return 0;
}
