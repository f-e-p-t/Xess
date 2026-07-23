#include "./fepttp-distrib/fepttp.h"
#include "eval.h"
#include "json.hpp"
#include <iostream>

using json = nlohmann::json;

Board UI_board = board;
int last_move_source_and_target[2] = {0};
void UpdateLastMoveSourceAndTarget(uint16_t move){
    int source = move & 0b0000000000111111;
    int target = (move >> 6) & 0b0000000000111111;
    last_move_source_and_target[0] = source; last_move_source_and_target[1] = target;
}

void InitialiseAll(){
    TT.SetSize(engine.transposition_table_size_MB);
    InitialiseZobristKeys();
    InitialiseHashKey();
    PrecomputePawnAttacksTable();
    PrecomputeKnightAttacksTable();
    PrecomputeBishopAttacksTable();
    PrecomputeRookAttacksTable();
    PrecomputeKingAttacksTable();
}

// Returns 0 is the move is illegal
uint16_t ConvertLANMoveToEngineMoveIfLegal(std::string LAN){
    uint16_t selected_move = 0;

    // Extract source, target and promotion type
    int source_file = LAN[0] - 'a';
    int source_rank = 8 - (LAN[1] - '0');
    int source = (8 * source_rank) + source_file;

    int target_file = LAN[2] - 'a';
    int target_rank = 8 - (LAN[3] - '0');
    int target = (8 * target_rank) + target_file;

    Piece promotion_type;
    switch(LAN[4]){
        case 'n': { promotion_type = Piece::knight; break; }
        case 'b': { promotion_type = Piece::bishop; break; }
        case 'r': { promotion_type = Piece::rook; break; }
        case 'q': { promotion_type = Piece::queen; break; }
        default: { promotion_type = Piece::NO_PIECE; break; }
    }

    MoveList list; GeneratePseudoLegalMoves(list);
    uint16_t move;
    for(int i = 0; i < list.count; i++){
        move = list.list[i];
        int move_source = move & 0b0000000000111111;
        int move_target = (move & 0b0000111111000000) >> 6;
        int move_flag = (move & 0b1111000000000000) >> 12;

        if(source == move_source && target == move_target){
            if(promotion_type == Piece::NO_PIECE && move_flag < 8){
                selected_move = move;
                break;
            }

            else{
                if(
                    promotion_type == Piece::knight &&
                    (move_flag == MoveFlag::quiet_knight_promo || move_flag == MoveFlag::capture_knight_promo)
                ){
                    selected_move = move; break;
                }

                else if(
                    promotion_type == Piece::bishop &&
                    (move_flag == MoveFlag::quiet_bishop_promo || move_flag == MoveFlag::capture_bishop_promo)
                ){
                    selected_move = move; break;
                }

                else if(
                    promotion_type == Piece::rook &&
                    (move_flag == MoveFlag::quiet_rook_promo || move_flag == MoveFlag::capture_rook_promo)
                ){
                    selected_move = move; break;
                }

                else if(
                    promotion_type == Piece::queen &&
                    (move_flag == MoveFlag::quiet_queen_promo || move_flag == MoveFlag::capture_queen_promo)
                ){
                    selected_move = move; break;
                }
            }
        }
    }

    // Full legality check
    if(selected_move){
        UnmakeMoveGameState irr_info = board.MakeMove(selected_move, board.to_move);
        if(board.InCheck(static_cast<Colour>(!board.to_move))){
            board.UnmakeMove(selected_move, board.to_move, irr_info);
            selected_move = 0;
        } else{
            board.UnmakeMove(selected_move, board.to_move, irr_info);
        }
    }

    return selected_move;
}

// Game end status for the side whose turn it is to move
enum GameEnd { in_play, checkmate, stalemate };
GameEnd GameFinishedType(){
    MoveList list; GeneratePseudoLegalMoves(list);
    int legal_moves = 0;
    for(int i = 0; i < list.count; i++){
        UnmakeMoveGameState irr_info = board.MakeMove(list.list[i], board.to_move);
        if(board.InCheck(static_cast<Colour>(!board.to_move))){
            board.UnmakeMove(list.list[i], board.to_move, irr_info);
            continue;
        } else{
            board.UnmakeMove(list.list[i], board.to_move, irr_info);
            legal_moves++;
        }
    }

    if(legal_moves){ return GameEnd::in_play; }

    else{
        return (board.InCheck(board.to_move) ? GameEnd::checkmate : GameEnd::stalemate);
    }
}

std::string GetUIBoardStringFromBoard(){
    std::string notFEN = "";
    for(int sq = 0; sq < 64; sq++){
        switch(UI_board.PieceAtSquare(sq, Colour::white)){
            case Piece::pawn: { notFEN += 'P'; continue; }
            case Piece::knight: { notFEN += 'N'; continue; }
            case Piece::bishop: { notFEN += 'B'; continue; }
            case Piece::rook: { notFEN += 'R'; continue; }
            case Piece::queen: { notFEN += 'Q'; continue; }
            case Piece::king: { notFEN += 'K'; continue; }
            default: { break; }
        }
        
        switch(UI_board.PieceAtSquare(sq, Colour::black)){
            case Piece::pawn: { notFEN += 'p'; break; }
            case Piece::knight: { notFEN += 'n'; break; }
            case Piece::bishop: { notFEN += 'b'; break; }
            case Piece::rook: { notFEN += 'r'; break; }
            case Piece::queen: { notFEN += 'q'; break; }
            case Piece::king: { notFEN += 'k'; break; }
            default: { notFEN += '-'; break; }
        }
    }

    return notFEN;
}

HTTPReqRes UICommunication(HTTPRequest req, HTTPResponse res){

    if(req.target == "/poll-board-update"){
        res.status_code = 200;
        json j;
        j["board"] = GetUIBoardStringFromBoard();
        j["source"] = last_move_source_and_target[0];
        j["target"] = last_move_source_and_target[1];
        res.ApplicationJSON(j.dump());
    }

    return HTTPReqRes(req, res);
}

int main(){
    Fepttp server = Fepttp(8000);
    server.chain.push_back(UICommunication);
    server.CORS.push_back("http://127.0.0.1:5500"); // <--- Add CORS header to allow reqs from 'Live Server' extension on port 5500

    // ------------

    engine.search_depth = 10;
    engine.transposition_table_size_MB = 1024;

    ParseFEN(FEN);
    InitialiseAll();
    PrintBoardToTerminal();
    UI_board = board;

    // ------------

    server.run();

    // Gameplay loop
    while(GameFinishedType() == GameEnd::in_play){
        
        // Player to move
        if(board.to_move == player_playing_as){
            std::string player_input; uint16_t player_move; bool legal_move_chosen = false;
            while(!legal_move_chosen){
                std::cout  << "Enter move: "; std::cin >> player_input;
                player_move = ConvertLANMoveToEngineMoveIfLegal(player_input);
                if(player_move){ legal_move_chosen = true; }
            }
            board.MakeMove(player_move, board.to_move);
            UI_board.MakeMove(player_move, UI_board.to_move);
            UpdateLastMoveSourceAndTarget(player_move);
        }

        // Engine to move
        else {
            engine.IterativeSearch(engine.search_depth);
            std::cout << "\nNodes searched: " << nodes_searched << "\n";
            engine.PrintPVToTerminal();

            board.MakeMove(PV_table[0][0], board.to_move);
            UI_board.MakeMove(PV_table[0][0], UI_board.to_move);
            UpdateLastMoveSourceAndTarget(PV_table[0][0]);

            memset(PV_table, 0, sizeof(PV_table));
            memset(PV_length, 0, sizeof(PV_length));
            memset(history_moves, 0, sizeof(history_moves));
            nodes_searched = 0;
        }

        PrintBoardToTerminal();
    }

    return 0;
}