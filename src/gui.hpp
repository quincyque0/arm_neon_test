#pragma once

namespace AppGui {
    struct Options {
        bool forceSoftware = false;
    };
    int Run(const Options& opts);
}
