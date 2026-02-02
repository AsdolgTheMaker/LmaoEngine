#include "core/Engine.h"
#include "core/Log.h"

int main() {
    lmao::Engine engine;

    if (!engine.init()) {
        LOG(Core, Error, "Failed to initialize engine");
        return 1;
    }

    engine.run();
    engine.shutdown();
    return 0;
}
