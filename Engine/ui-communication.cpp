#include "./fepttp-distrib/fepttp.h"
#include "board.h"
#include <iostream>

void InitialiseAll(){
    PrecomputeKnightAttacks();
    PrecomputeBishopAttacksTable();
    PrecomputeRookAttacksTable();
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
    GenerateMoves(list);
    PrintMoveListToTerminal(list);

    //std::bitset<64> x(rook_attacks[36][HashRookOccConfig(36, rook_rays_no_edges_occ_configs[36][4095])]);
    //std::cout << "\n" << x << "\n\n";

    //std::bitset<64> x(bishop_attacks[36][HashBishopOccConfig(36, bishop_rays_no_edges_occ_configs[36][511])]);
    //std::cout << "\n" << x << "\n\n";

    // ---

    server.run();

    return 0;
}