#include "./fepttp-distrib/fepttp.h"
#include "eval.h"
#include "json.hpp"
#include <iostream>

using json = nlohmann::json;

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
    int source_rank = LAN[1] - '0';
    int source = (8 * source_rank) + source_file;

    int target_file = LAN[2] - 'a';
    int target_rank = LAN[3] - '0';
    int target = (8 * target_rank) + target_file;

    Piece promotion_type;
    switch(LAN[4]){
        case 'n': { promotion_type = Piece::knight; }
        case 'b': { promotion_type = Piece::bishop; }
        case 'r': { promotion_type = Piece::rook; }
        case 'q': { promotion_type = Piece::queen; }
        default: { promotion_type = Piece::NO_PIECE; }
    }

    MoveList list; GeneratePseudoLegalMoves(list);
    uint16_t move;
    for(int i = 0; i < list.count; i++){
        move = list.list[i];
        int move_source = move & 0b0000000000111111;
        int move_target = (move & 0b0000111111000000) >> 6;
        int move_flag = (move & 0b1111000000000000) >> 12;

        if(source == move_source && target == move_target){
            if(promotion_type == Piece::NO_PIECE){
                //
            }
        }
    }

    return selected_move;
}

std::string GetUIBoardStringFromBoard(){
    std::string notFEN = "";
    for(int sq = 0; sq < 64; sq++){
        switch(board.PieceAtSquare(sq, Colour::white)){
            case Piece::pawn: { notFEN += 'P'; continue; }
            case Piece::knight: { notFEN += 'N'; continue; }
            case Piece::bishop: { notFEN += 'B'; continue; }
            case Piece::rook: { notFEN += 'R'; continue; }
            case Piece::queen: { notFEN += 'Q'; continue; }
            case Piece::king: { notFEN += 'K'; continue; }
            default: { break; }
        }
        
        switch(board.PieceAtSquare(sq, Colour::black)){
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
    HTTPReqRes pair = HTTPReqRes(req, res);

    // Handle a board UI update request

    return pair;
}

int main(){
    Fepttp server = Fepttp(8000);
    server.chain.push_back(UICommunication);
    server.CORS.push_back("http://127.0.0.1:5500"); // <--- Add CORS header to allow play from 'Live Server' extension on port 5500

    // ------------

    engine.search_depth = 12;
    engine.transposition_table_size_MB = 512;

    ParseFEN(FEN);
    InitialiseAll();
    PrintBoardToTerminal();

    // when creating gp loop, remember to clear things, like nodecount and PV

    // ---

    server.run();

    return 0;
}