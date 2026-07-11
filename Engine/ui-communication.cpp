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
    engine.transposition_table_size_MB = 1024;

    ParseFEN(FEN);
    InitialiseAll();
    PrintBoardToTerminal();

    //perft(6); std::cout << nodes << "\n";

    std::cout << engine.Search(engine.search_depth, -INFTY, INFTY, 0) << "\n";
    std::cout << "\n" << nodes << "\n";
    PrintMoveToTerminal(best_move_temp);

    // ---

    server.run();

    return 0;
}