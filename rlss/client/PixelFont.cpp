#include "PixelFont.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <functional>
#include <string_view>

namespace rlss::client::font {

namespace {

/** @brief One glyph: a row per string, '#' for a lit square. */
using Glyph = std::array<std::string_view, GLYPH_ROWS>;

/** @brief A character and its glyph. */
struct Entry {
    /** @brief Character. */
    char character = ' ';
    /** @brief Its glyph. */
    Glyph glyph;
};

constexpr auto GLYPHS = std::to_array<Entry>({
    {.character = 'A', .glyph = {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {.character = 'B', .glyph = {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
    {.character = 'C', .glyph = {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}},
    {.character = 'D', .glyph = {"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."}},
    {.character = 'E', .glyph = {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
    {.character = 'F', .glyph = {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}},
    {.character = 'G', .glyph = {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"}},
    {.character = 'H', .glyph = {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {.character = 'I', .glyph = {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {.character = 'J', .glyph = {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
    {.character = 'K', .glyph = {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}},
    {.character = 'L', .glyph = {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
    {.character = 'M', .glyph = {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
    {.character = 'N', .glyph = {"#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"}},
    {.character = 'O', .glyph = {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {.character = 'P', .glyph = {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
    {.character = 'Q', .glyph = {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"}},
    {.character = 'R', .glyph = {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
    {.character = 'S', .glyph = {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
    {.character = 'T', .glyph = {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
    {.character = 'U', .glyph = {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {.character = 'V', .glyph = {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
    {.character = 'W', .glyph = {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."}},
    {.character = 'X', .glyph = {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}},
    {.character = 'Y', .glyph = {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}},
    {.character = 'Z', .glyph = {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"}},
    {.character = '0', .glyph = {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
    {.character = '1', .glyph = {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {.character = '2', .glyph = {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
    {.character = '3', .glyph = {"#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."}},
    {.character = '4', .glyph = {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
    {.character = '5', .glyph = {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
    {.character = '6', .glyph = {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}},
    {.character = '7', .glyph = {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
    {.character = '8', .glyph = {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
    {.character = '9', .glyph = {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."}},
    {.character = ':', .glyph = {".....", "..#..", "..#..", ".....", "..#..", "..#..", "....."}},
    {.character = '-', .glyph = {".....", ".....", ".....", ".###.", ".....", ".....", "....."}},
    {.character = '+', .glyph = {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."}},
    {.character = '/', .glyph = {"....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."}},
    {.character = '.', .glyph = {".....", ".....", ".....", ".....", ".....", ".##..", ".##.."}},
    {.character = '!', .glyph = {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."}},
    {.character = '?', .glyph = {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."}},
    {.character = '\'', .glyph = {"..#..", "..#..", ".#...", ".....", ".....", ".....", "....."}},
});

/** @brief Glyph of a character, or nothing (drawn as a space). */
const Glyph* glyphOf(const char character) {
    const auto upper = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    for (const Entry& entry : GLYPHS) {
        if (entry.character == upper) {
            return &entry.glyph;
        }
    }
    return nullptr;
}

}  // namespace

int width(const std::string_view text) noexcept {
    if (text.empty()) {
        return 0;
    }
    return (static_cast<int>(text.size()) * ADVANCE) - 1;
}

void forEachRun(const std::string_view text, const std::function<void(const Run&)>& draw) {
    for (std::size_t index = 0; index < text.size(); ++index) {
        const Glyph* glyph = glyphOf(text.at(index));
        if (glyph == nullptr) {
            continue;
        }
        const int left = static_cast<int>(index) * ADVANCE;
        for (int row = 0; row < GLYPH_ROWS; ++row) {
            const std::string_view line = glyph->at(static_cast<std::size_t>(row));
            int column = 0;
            while (column < GLYPH_COLUMNS) {
                if (line.at(static_cast<std::size_t>(column)) != '#') {
                    ++column;
                    continue;
                }
                const int start = column;
                while (column < GLYPH_COLUMNS && line.at(static_cast<std::size_t>(column)) == '#') {
                    ++column;
                }
                draw(Run{.column = left + start, .row = row, .length = column - start});
            }
        }
    }
}

}  // namespace rlss::client::font
