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
    //PrintMoveListToTerminal(list);

    //UnmakeMoveGameState x = board.MakeMove(list.list[11], board.to_move);
    //PrintBoardToTerminal();
    //board.UnmakeMove(list.list[11], board.to_move, x);
    //PrintBoardToTerminal();

    perft(6);
    std::cout << "\n" << nodes << "\n";
    //std::cout << "\n" << promos << "\n";
    //std::cout << "\n" << captures << "\n";
    //std::cout << "\n" << EP_captures << "\n";

    // ---

    server.run();

    return 0;
}