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

    struct PosRecord {
        uint64_t key;
        int16_t  score;
        uint16_t move;
        int8_t   result; 
    };

    void start(Engine& engine, std::string options) {
        int nodesLimit = 5000;
        int gamesCount = 1000;
        int randomMoves = 8;
        
        std::istringstream is(options);
        std::string token;
        while (is >> token) {
            if (token == "nodes") is >> nodesLimit;
            if (token == "games") is >> gamesCount;
        }

        // VÔ HIỆU HÓA TẤT CẢ CÁC KÊNH OUTPUT ĐỂ IM LẶNG TUYỆT ĐỐI
        engine.set_on_update_full([](const Engine::InfoFull&) {});
        engine.set_on_update_no_moves([](const Engine::InfoShort&) {});
        engine.set_on_iter([](const Engine::InfoIter&) {});
        engine.set_on_bestmove([](std::string_view, std::string_view) {});
        engine.set_on_verify_networks([](std::string_view) {}); // Tắt info string NNUE

        std::cout << "Starting Silent Production Datagen: Nodes=" << nodesLimit << ", Games=" << gamesCount << std::endl;
        std::ofstream outfile("nextfish_data.binpack", std::ios::binary | std::ios::app);
        PRNG rng(now());

        for (int g = 1; g <= gamesCount; ++g) {
            engine.set_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {});
            std::vector<PosRecord> gameHistory;
            int8_t finalResult = 0;

            for (int i = 0; i < randomMoves; ++i) {
                MoveList<LEGAL> moves(engine.get_position());
                if (moves.size() == 0) break;
                Move m = *(moves.begin() + (rng.rand<size_t>() % moves.size()));
                engine.set_position(engine.get_position().fen(), {UCIEngine::move(m, engine.get_position().is_chess960())});
            }

            int ply = 0;
            while (ply++ < 200) {
                Search::LimitsType limits;
                limits.nodes = nodesLimit;
                
                engine.go(limits);
                engine.wait_for_search_finished();

                auto& rootMoves = engine.main_thread()->worker->rootMoves;
                if (rootMoves.empty()) break;

                Move bestMove = rootMoves[0].pv[0];
                Value score = rootMoves[0].score;

                PosRecord rec;
                rec.key = engine.get_position().key();
                rec.score = (int16_t)score;
                rec.move = bestMove.raw();
                gameHistory.push_back(rec);

                engine.set_position(engine.get_position().fen(), {UCIEngine::move(bestMove, engine.get_position().is_chess960())});

                if (is_win(score)) {
                    finalResult = (engine.get_position().side_to_move() == WHITE ? -1 : 1);
                    break;
                }
                if (is_loss(score)) {
                    finalResult = (engine.get_position().side_to_move() == WHITE ? 1 : -1);
                    break;
                }
                if (engine.get_position().is_draw(ply)) break;
            }

            for (auto& rec : gameHistory) {
                rec.result = finalResult;
                outfile.write(reinterpret_cast<char*>(&rec), sizeof(PosRecord));
            }
            
            // In đè trên một dòng duy nhất
            std::cout << "\r[Progress] Game " << g << "/" << gamesCount 
                      << " | Plys: " << ply 
                      << " | Result: " << (finalResult == 1 ? "1-0" : (finalResult == -1 ? "0-1" : "1/2"))
                      << " | Total: " << (outfile.tellp() / 1024) << " KB          " << std::flush;
            
            if (g % 10 == 0) outfile.flush();
        }
        std::cout << "\nDatagen finished successfully." << std::endl;
    }
}