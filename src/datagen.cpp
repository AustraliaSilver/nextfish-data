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
        int nodesLimit = 3000;
        int gamesCount = 1000;
        
        std::istringstream is(options);
        std::string token;
        while (is >> token) {
            if (token == "nodes") is >> nodesLimit;
            if (token == "games") is >> gamesCount;
        }

        engine.set_on_update_full([](const Engine::InfoFull&) {});
        engine.set_on_update_no_moves([](const Engine::InfoShort&) {});
        engine.set_on_iter([](const Engine::InfoIter&) {});
        engine.set_on_bestmove([](std::string_view, std::string_view) {});
        engine.set_on_verify_networks([](std::string_view) {});

        std::cout << "SF-Style Datagen Active. Optimizing for nnue-pytorch..." << std::endl;
        std::ofstream outfile("nextfish_data.binpack", std::ios::binary | std::ios::app);
        PRNG rng(now());

        for (int g = 1; g <= gamesCount; ++g) {
            engine.set_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {});
            std::vector<PackedPos> gameHistory;
            int8_t gameResult = 0; 

            // 1. Khai cuộc ngẫu nhiên (Dựa trên 8-12 nước)
            int rMoves = 8 + (rng.rand<int>() % 5);
            for (int i = 0; i < rMoves; ++i) {
                MoveList<LEGAL> moves(engine.get_position());
                if (moves.size() == 0) break;
                Move m = *(moves.begin() + (rng.rand<size_t>() % moves.size()));
                engine.set_position(engine.get_position().fen(), {UCIEngine::move(m, engine.get_position().is_chess960())});
            }

            // 2. Engine tự đấu với cơ chế ngẫu nhiên nhẹ (Epsilon)
            int ply = 0;
            while (ply++ < 200) {
                Search::LimitsType limits;
                limits.nodes = nodesLimit;
                engine.go(limits);
                engine.wait_for_search_finished();

                auto& rootMoves = engine.main_thread()->worker->rootMoves;
                if (rootMoves.empty()) break;

                // Chọn nước đi (Epsilon-greedy: 10% chọn nước gần tốt nhất)
                int moveIdx = 0;
                if (rootMoves.size() > 1 && (rng.rand<int>() % 100 < 10)) {
                    if (std::abs(rootMoves[0].score - rootMoves[1].score) < 30) moveIdx = 1;
                }

                Move bestMove = rootMoves[moveIdx].pv[0];
                Value score = rootMoves[moveIdx].score;

                // Nén bàn cờ (4 bits per square)
                PackedPos rec;
                std::memset(rec.board, 0, 32);
                for (int i = 0; i < 64; ++i) {
                    uint8_t pc = (uint8_t)engine.get_position().piece_on(Square(i));
                    if (i % 2 == 0) rec.board[i/2] |= (pc & 0x0F);
                    else rec.board[i/2] |= ((pc & 0x0F) << 4);
                }
                
                rec.side = (uint8_t)engine.get_position().side_to_move();
                rec.score = (int16_t)score;
                rec.move = bestMove.raw();
                gameHistory.push_back(rec);

                engine.set_position(engine.get_position().fen(), {UCIEngine::move(bestMove, engine.get_position().is_chess960())});

                if (is_win(score)) { gameResult = (rec.side == WHITE ? 1 : -1); break; }
                if (is_loss(score)) { gameResult = (rec.side == WHITE ? -1 : 1); break; }
                if (engine.get_position().is_draw(ply)) break;
            }

            // 3. Gán kết quả thực tế cho toàn bộ lịch sử ván đấu và ghi file
            for (auto& rec : gameHistory) {
                rec.result = gameResult;
                outfile.write(reinterpret_cast<char*>(&rec), sizeof(PackedPos));
            }
            
            if (g % 1 == 0) {
                std::cout << "\r[SF-Datagen] Game " << g << "/" << gamesCount 
                          << " | Storage: " << (outfile.tellp() / 1024) << " KB"
                          << " | Res: " << (gameResult == 1 ? "1-0" : (gameResult == -1 ? "0-1" : "1/2"))
                          << "          " << std::flush;
            }
            if (g % 20 == 0) outfile.flush();
        }
        std::cout << "\nProduction run complete." << std::endl;
    }
}