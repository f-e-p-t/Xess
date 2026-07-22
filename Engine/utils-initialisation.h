#include <iostream>
#include <random>

typedef std::uint64_t u64;

std::mt19937_64 rng;
u64 RandomU64(){
    return rng();
}

// |---------------------|
// | Constants and Enums |---------------------------------------------------
// |---------------------|

constexpr int PAWN_VALUE_CTP = 100;
constexpr int KNIGHT_VALUE_CTP = 320;
constexpr int BISHOP_VALUE_CTP = 330;
constexpr int ROOK_VALUE_CTP = 500;
constexpr int QUEEN_VALUE_CTP = 900;

// This can shrink when there is a stronger evaluation
constexpr int DELTA = 300;

// For the triangular PV table
constexpr int MAX_PLY = 64;

constexpr int INFTY = 1000000000;
constexpr int CHECKMATE = 1000000;
constexpr int CHECKMATE_THRESHOLD = 900000;
constexpr int STALEMATE = 0;

enum Piece { pawn, knight, bishop, rook, queen, king, NO_PIECE };
enum Colour { white, black };
enum Square { // Top-left ---> bottom-right as read
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1,
    NO_SQUARE
};
enum CastlingRights { white_ks, white_qs, black_ks, black_qs };
enum MoveFlag { 
    quiet_move, double_pawn_push, KS_castle, QS_castle, normal_capture, EP_capture, six, seven, quiet_knight_promo, quiet_bishop_promo, quiet_rook_promo, quiet_queen_promo,
    capture_knight_promo, capture_bishop_promo, capture_rook_promo, capture_queen_promo
};
enum TEntryFlag { exact, UB, LB };

int PieceValue(Piece piece){
    switch(piece){
        case Piece::pawn: { return PAWN_VALUE_CTP; }
        case Piece::knight: { return KNIGHT_VALUE_CTP; }
        case Piece::bishop: { return BISHOP_VALUE_CTP; }
        case Piece::rook: { return ROOK_VALUE_CTP; }
        case Piece::queen: { return QUEEN_VALUE_CTP; }
        default: { return 0; }
    }
}

// |--------------|
// | Zobrist Keys |------------------------------------------------------------
// |--------------|

u64 piece_keys[2][6][64];

u64 EP_keys[64];

// 16 unique combinations of castling rights
u64 castling_keys[16];

u64 side_key;

void InitialiseZobristKeys(){
    for(int colour = 0; colour < 2; colour++){
        for(int piece = 0; piece < 6; piece++){
            for(int sq = 0; sq < 64; sq++){
                piece_keys[colour][piece][sq] = RandomU64();
            }
        }
    }

    for(int sq = 0; sq < 64; sq++){
        EP_keys[sq] = RandomU64();
    }

    for(int cr = 0; cr < 16; cr++){
        castling_keys[cr] = RandomU64();
    }

    side_key = RandomU64();
}