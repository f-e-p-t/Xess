#include "./fepttp-distrib/fepttp.h"
#include "board.h"
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

    InitialiseAll();
    ParseFEN(FEN);
    PrintBoardToTerminal();

    MoveList list;
    GeneratePseudoLegalMoves(list);
    PrintMoveListToTerminal(list);

    // ---

    server.run();

    return 0;
}