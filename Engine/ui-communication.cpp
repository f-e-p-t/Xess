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

    engine.search_depth = 10;
    engine.transposition_table_size_MB = 512;

    ParseFEN(FEN);
    InitialiseAll();
    PrintBoardToTerminal();

    //perft(6); std::cout << nodes << "\n";

    //MoveList list; GeneratePseudoLegalMoves(list);
    //PrintMoveListToTerminal(list);
    //ScoreMoveList(list, (uint16_t)0);
    //PrepareBestMove(list, 0);
    //std::cout << "\n\n";
    //PrintMoveListToTerminal(list);

    //std::cout << engine.Search(engine.search_depth, -INFTY, INFTY, 0) << "\n";
    
    engine.IterativeSearch(engine.search_depth);
    std::cout << "\nNodes searched: " << nodes_searched << "\n";
    engine.PrintPV();

    // ---

    server.run();

    return 0;
}