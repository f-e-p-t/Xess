#include "./fepttp-distrib/fepttp.h"
#include "eval.h"
#include <iostream>

void InitialiseAll(){
    PrecomputePawnAttacksTable();
    PrecomputeKnightAttacksTable();
    PrecomputeBishopAttacksTable();
    PrecomputeRookAttacksTable();
    PrecomputeKingAttacksTable();
}

HTTPReqRes UICommunication(HTTPRequest req, HTTPResponse res){
    res.status_code = 200;

    return HTTPReqRes(req, res);
}

int main(){
    Fepttp server = Fepttp(8000);

    server.chain.push_back(UICommunication);

    // ---

    ParseFEN(FEN);
    InitialiseAll();
    engine.search_depth = 6;
    PrintBoardToTerminal();


    std::cout << engine.Search(engine.search_depth, -INFTY, INFTY) << "\n";
    std::cout << "\n" << nodes << "\n";
    PrintMoveToTerminal(best_move);

    // ---

    server.run();

    return 0;
}