#include "./fepttp-distrib/fepttp.h"
#include "eval.h"
#include <iostream>

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

// Gameplay loop
HTTPReqRes UICommunication(HTTPRequest req, HTTPResponse res){
    res.status_code = 200;

    return HTTPReqRes(req, res);
}

int main(){
    Fepttp server = Fepttp(8000);

    server.chain.push_back(UICommunication);

    // ---

    engine.search_depth = 7;
    engine.transposition_table_size_MB = 512;

    ParseFEN(FEN);
    InitialiseAll();
    PrintBoardToTerminal();

    //perft(6); std::cout << nodes << "\n";

    //MoveList list; GeneratePseudoLegalMoves(list);
    //PrintMoveListToTerminal(list); std::cout << "\n";
    //std::cout << board.hash_key << "\n";
    //UnmakeMoveGameState x = board.MakeMove(list.list[10], board.to_move);
    //std::cout << board.hash_key << "\n";
    //board.UnmakeMove(list.list[10], board.to_move, x);
    //std::cout << board.hash_key << "\n";

    //std::cout << engine.Search(engine.search_depth, -INFTY, INFTY) << "\n";
    //std::cout << "\n" << nodes << "\n";
    //PrintMoveToTerminal(best_move);

    // ---

    server.run();

    return 0;
}