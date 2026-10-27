#include <gtest/gtest.h>

#include <algorithm>
#include <variant>

#include "ecs/Registry.hpp"
#include "ui/components/FontId.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"
#include "ui/system/UiDrawItem.hpp"

TEST(FontId, NoFontSentinelIsZero) {
    EXPECT_EQ(ui::NO_FONT, 0U);
}

TEST(UiAnchor, DefaultValues) {
    const ui::UiAnchor anchor;

    EXPECT_EQ(anchor.anchorX, 0.0F);
    EXPECT_EQ(anchor.anchorY, 0.0F);
    EXPECT_EQ(anchor.offsetX, 0.0F);
    EXPECT_EQ(anchor.offsetY, 0.0F);
}

TEST(UiAnchor, DesignatedInitializersKeepOtherDefaults) {
    const ui::UiAnchor anchor{.anchorX = 0.5F, .anchorY = 1.0F};

    EXPECT_EQ(anchor.anchorX, 0.5F);
    EXPECT_EQ(anchor.anchorY, 1.0F);
    EXPECT_EQ(anchor.offsetX, 0.0F);
    EXPECT_EQ(anchor.offsetY, 0.0F);
}

TEST(UiAnchor, WorksAsEcsComponent) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();

    registry.addComponent<ui::UiAnchor>(entity, ui::UiAnchor{.anchorX = 0.5F, .offsetY = -8.0F});

    ASSERT_TRUE(registry.hasComponent<ui::UiAnchor>(entity));
    EXPECT_EQ(registry.getComponents<ui::UiAnchor>().get(entity).anchorX, 0.5F);
    EXPECT_EQ(registry.getComponents<ui::UiAnchor>().get(entity).offsetY, -8.0F);
}

static std::pair<float, float> resolveAnchor(const ui::UiAnchor& anchor, float screenW, float screenH) {
    return {(anchor.anchorX * screenW) + anchor.offsetX, (anchor.anchorY * screenH) + anchor.offsetY};
}

TEST(UiAnchorResolution, TopLeftCorner) {
    const ui::UiAnchor anchor{.anchorX = 0.0F, .anchorY = 0.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F);

    EXPECT_FLOAT_EQ(x, 0.0F);
    EXPECT_FLOAT_EQ(y, 0.0F);
}

TEST(UiAnchorResolution, BottomRightCorner) {
    const ui::UiAnchor anchor{.anchorX = 1.0F, .anchorY = 1.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F);

    EXPECT_FLOAT_EQ(x, 1920.0F);
    EXPECT_FLOAT_EQ(y, 1080.0F);
}

TEST(UiAnchorResolution, CenterWithOffset) {
    const ui::UiAnchor anchor{.anchorX = 0.5F, .anchorY = 0.5F, .offsetX = -50.0F, .offsetY = 10.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F);

    EXPECT_FLOAT_EQ(x, 910.0F);
    EXPECT_FLOAT_EQ(y, 550.0F);
}

TEST(UiRect, DefaultValues) {
    const ui::UiRect rect;

    EXPECT_EQ(rect.width, 0.0F);
    EXPECT_EQ(rect.height, 0.0F);
    EXPECT_EQ(rect.borderThickness, 0.0F);
    EXPECT_EQ(rect.borderColor.a, 0);
}

TEST(UiRect, DesignatedInitializersKeepOtherDefaults) {
    const ui::UiRect rect{
        .width = 200.0F,
        .height = 40.0F,
        .fillColor = {.r = 255, .g = 0, .b = 0, .a = 255},
    };

    EXPECT_EQ(rect.width, 200.0F);
    EXPECT_EQ(rect.height, 40.0F);
    EXPECT_EQ(rect.fillColor.r, 255);
    EXPECT_EQ(rect.borderThickness, 0.0F);
}

TEST(UiText, DefaultValues) {
    const ui::UiText text;

    EXPECT_TRUE(text.text.empty());
    EXPECT_EQ(text.fontSize, 20.0F);
    EXPECT_EQ(text.fontId, ui::NO_FONT);
}

TEST(UiText, DesignatedInitializersKeepOtherDefaults) {
    const ui::UiText text{.text = "score: 0", .fontSize = 32.0F};

    EXPECT_EQ(text.text, "score: 0");
    EXPECT_EQ(text.fontSize, 32.0F);
    EXPECT_EQ(text.fontId, ui::NO_FONT);
}

TEST(UiProgressBar, DefaultValues) {
    const ui::UiProgressBar bar;

    EXPECT_EQ(bar.value, 0.0F);
    EXPECT_EQ(bar.min, 0.0F);
    EXPECT_EQ(bar.max, 1.0F);
    EXPECT_EQ(bar.width, 100.0F);
    EXPECT_EQ(bar.height, 16.0F);
}

TEST(UiProgressBar, DesignatedInitializersKeepOtherDefaults) {
    const ui::UiProgressBar bar{.value = 50.0F, .min = 0.0F, .max = 100.0F};

    EXPECT_EQ(bar.value, 50.0F);
    EXPECT_EQ(bar.max, 100.0F);
    EXPECT_EQ(bar.width, 100.0F);
}

static float computeRatio(const ui::UiProgressBar& bar) {
    const float range = bar.max - bar.min;
    if (range <= 0.0F) {
        return 0.0F;
    }
    return std::clamp((bar.value - bar.min) / range, 0.0F, 1.0F);
}

TEST(UiProgressBarRatio, MidRange) {
    const ui::UiProgressBar bar{.value = 50.0F, .min = 0.0F, .max = 100.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 0.5F);
}

TEST(UiProgressBarRatio, ClampsBelowMin) {
    const ui::UiProgressBar bar{.value = -10.0F, .min = 0.0F, .max = 100.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 0.0F);
}

TEST(UiProgressBarRatio, ClampsAboveMax) {
    const ui::UiProgressBar bar{.value = 150.0F, .min = 0.0F, .max = 100.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 1.0F);
}

TEST(UiProgressBarRatio, ZeroRangeReturnsZero) {
    const ui::UiProgressBar bar{.value = 5.0F, .min = 5.0F, .max = 5.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 0.0F);
}

TEST(UiDrawItem, HoldsUiRect) {
    const ui::UiDrawItem item{
        .entity = 1,
        .screenX = 100.0F,
        .screenY = 200.0F,
        .drawable = ui::UiRect{.width = 50.0F, .height = 20.0F},
    };

    ASSERT_TRUE(std::holds_alternative<ui::UiRect>(item.drawable));
    EXPECT_EQ(std::get<ui::UiRect>(item.drawable).width, 50.0F);
}

TEST(UiDrawItem, HoldsUiText) {
    const ui::UiDrawItem item{
        .entity = 2,
        .screenX = 0.0F,
        .screenY = 0.0F,
        .drawable = ui::UiText{.text = "HP"},
    };

    ASSERT_TRUE(std::holds_alternative<ui::UiText>(item.drawable));
    EXPECT_EQ(std::get<ui::UiText>(item.drawable).text, "HP");
}

TEST(UiDrawItem, HoldsUiProgressBar) {
    const ui::UiDrawItem item{
        .entity = 3,
        .screenX = 0.0F,
        .screenY = 0.0F,
        .drawable = ui::UiProgressBar{.value = 75.0F, .max = 100.0F},
    };

    ASSERT_TRUE(std::holds_alternative<ui::UiProgressBar>(item.drawable));
    EXPECT_EQ(std::get<ui::UiProgressBar>(item.drawable).value, 75.0F);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
