#include <pxa/app.hpp>

using namespace pxa::ui;

// Test fixture only. Warm the clock's digits at every system text size so
// comparisons at different wall-clock times do not measure new glyph caches.
struct FontWarmup {
    auto view() {
        return Column(
            Text("0123456789:").font(Font::display),
            Text("0123456789:").font(Font::headline),
            Text("0123456789:").font(Font::title),
            Text("0123456789:").font(Font::body),
            Text("0123456789:").font(Font::label),
            Text("0123456789:").font(Font::caption)
        );
    }
};

PXA_APPLICATION(FontWarmup)
