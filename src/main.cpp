#include "cli.hpp"
#include "gui.hpp"
#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    bool forceCli = false;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--cli") == 0) forceCli = true;
    }

#ifdef HAVE_GUI
    if (!forceCli) {
        AppGui::Options opts;
        opts.forceSoftware = true;
        int rc = AppGui::Run(opts);
        if (rc == 0) return 0;
        std::cerr << "falling back to CLI...\n";
    }
#endif

    return AppCli::Run();
}
