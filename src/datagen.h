#ifndef DATAGEN_H_INCLUDED
#define DATAGEN_H_INCLUDED

#include <string>

namespace Stockfish {
    class Engine;
    namespace Datagen {
        void start(Engine& engine, std::string options);
    }
}

#endif