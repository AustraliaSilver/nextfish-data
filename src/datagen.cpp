#include "datagen.h"
#include "engine.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "uci.h"
#include "misc.h"
#include "movegen.h"
#include <fstream>
#include <iostream>
#include <vector>

namespace Stockfish::Datagen {

    // Định dạng nén giống cách Stockfish/nnue-pytorch xử lý
    struct PackedPos {
        uint8_t  board[32]; // 4 bits per square -> 32 bytes total
        int16_t  score;     
        uint16_t move;      
        int8_t   result;    
        uint8_t  side;      
    };

    void start(Engine& engine, std::string options) {
        int nodesLimit = 8000;
        int gamesCount = 1000;
        std::string outputFileName = "nextfish_data.binpack";
        std::string bookFile = "";
        
        std::istringstream is(options);
        std::string token;
        while (is >> token) {
            if (token == "nodes") is >> nodesLimit;
            if (token == "games") is >> gamesCount;
            if (token == "out") is >> outputFileName;
            if (token == "book") is >> bookFile;
        }

        std::vector<std::string> bookLines;
        if (!bookFile.empty()) {
            std::ifstream bf(bookFile);
            std::string line;
            while (std::getline(bf, line)) {
                if (!line.empty()) bookLines.push_back(line);
            }
            std::cout << "Loaded " << bookLines.size() << " lines from book." << std::endl;
        }

        engine.set_on_update_full([](const Engine::InfoFull&) {});
        engine.set_on_update_no_moves([](const Engine::InfoShort&) {});
        engine.set_on_iter([](const Engine::InfoIter&) {});
        engine.set_on_bestmove([](std::string_view, std::string_view) {});
        engine.set_on_verify_networks([](std::string_view) {});

        std::cout << "SF-Style Datagen Active. Nodes: " << nodesLimit << " | Output: " << outputFileName << std::endl;
        std::ofstream outfile(outputFileName, std::ios::binary | std::ios::app);
        PRNG rng(now());

        for (int g = 1; g <= gamesCount; ++g) {
            if (!bookLines.empty()) {
                std::string line = bookLines[rng.rand<size_t>() % bookLines.size()];
                std::istringstream ls(line);
                std::vector<std::string> moves;
                std::string m;
                while (ls >> m) moves.push_back(m);
                engine.set_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", moves);
            } else {
                engine.set_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {});
                // 1. Khai cuộc ngẫu nhiên (Dựa trên 8-12 nước) nếu không có book
                int rMoves = 8 + (rng.rand<int>() % 5);
                for (int i = 0; i < rMoves; ++i) {
                    MoveList<LEGAL> moves(engine.get_position());
                    if (moves.size() == 0) break;
                    Move m = *(moves.begin() + (rng.rand<size_t>() % moves.size()));
                    engine.set_position(engine.get_position().fen(), {UCIEngine::move(m, engine.get_position().is_chess960())});
                }
            }

            std::vector<PackedPos> gameHistory;
}