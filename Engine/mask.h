#include <iostream>
#include <cstdint>
#include <bitset>
#include <random>
#include <vector>
#include <algorithm>

typedef std::uint64_t u64;

const u64 FILE_A = 0b0000000100000001000000010000000100000001000000010000000100000001;
const u64 FILE_B = 0b0000001000000010000000100000001000000010000000100000001000000010;
const u64 FILE_G = 0b0100000001000000010000000100000001000000010000000100000001000000;
const u64 FILE_H = 0b1000000010000000100000001000000010000000100000001000000010000000;
const u64 NOT_FILE_A = ~FILE_A;
const u64 NOT_FILE_AB = ~(FILE_A | FILE_B);
const u64 NOT_FILE_H = ~FILE_H;
const u64 NOT_FILE_GH = ~(FILE_G | FILE_H);
const u64 RANK_1 = 0b1111111100000000000000000000000000000000000000000000000000000000;
const u64 RANK_8 = 0b0000000000000000000000000000000000000000000000000000000011111111;
const u64 NOT_RANK_1 = ~RANK_1;
const u64 NOT_RANK_8 = ~RANK_8;

// Bitboard utilities
#define GetBitBinary(bitboard, square) (!!((bitboard) & (1ULL << (square))))

void SetBit(u64& bitboard, int square){ // To 1
    ((bitboard) |= (1ULL << (square)));
}

u64 GetBit(u64 bitboard, int square){
    return ((bitboard) & (1ULL << (square)));
}

void PopBit(u64& bitboard, int square){ // To 0
    ((bitboard) &= ~(1ULL << (square)));
}

int GetLSBitIndex(u64 bitboard){
    return __builtin_ctzll(bitboard);
}

int NumberOfNonZeroBits(u64 bitboard){
    int count = 0;

    while(bitboard){
        count++;
        GetLSBitIndex(bitboard);
        bitboard &= bitboard - 1;
    }

    return count;
}

u64 knight_attacks[64];

// ---

u64 bishop_rays_no_edges[64];
u64 bishop_rays_no_edges_occ_configs[64][512];
u64 bishop_magic_numbers[64] = {
    0x40040844404084ULL,
    0x2004208a004208ULL,
    0x10190041080202ULL,
    0x108060845042010ULL,
    0x581104180800210ULL,
    0x2112080446200010ULL,
    0x1080820820060210ULL,
    0x3c0808410220200ULL,
    0x4050404440404ULL,
    0x21001420088ULL,
    0x24d0080801082102ULL,
    0x1020a0a020400ULL,
    0x40308200402ULL,
    0x4011002100800ULL,
    0x401484104104005ULL,
    0x801010402020200ULL,
    0x400210c3880100ULL,
    0x404022024108200ULL,
    0x810018200204102ULL,
    0x4002801a02003ULL,
    0x85040820080400ULL,
    0x810102c808880400ULL,
    0xe900410884800ULL,
    0x8002020480840102ULL,
    0x220200865090201ULL,
    0x2010100a02021202ULL,
    0x152048408022401ULL,
    0x20080002081110ULL,
    0x4001001021004000ULL,
    0x800040400a011002ULL,
    0xe4004081011002ULL,
    0x1c004001012080ULL,
    0x8004200962a00220ULL,
    0x8422100208500202ULL,
    0x2000402200300c08ULL,
    0x8646020080080080ULL,
    0x80020a0200100808ULL,
    0x2010004880111000ULL,
    0x623000a080011400ULL,
    0x42008c0340209202ULL,
    0x209188240001000ULL,
    0x400408a884001800ULL,
    0x110400a6080400ULL,
    0x1840060a44020800ULL,
    0x90080104000041ULL,
    0x201011000808101ULL,
    0x1a2208080504f080ULL,
    0x8012020600211212ULL,
    0x500861011240000ULL,
    0x180806108200800ULL,
    0x4000020e01040044ULL,
    0x300000261044000aULL,
    0x802241102020002ULL,
    0x20906061210001ULL,
    0x5a84841004010310ULL,
    0x4010801011c04ULL,
    0xa010109502200ULL,
    0x4a02012000ULL,
    0x500201010098b028ULL,
    0x8040002811040900ULL,
    0x28000010020204ULL,
    0x6000020202d0240ULL,
    0x8918844842082200ULL,
    0x4010011029020020ULL
};
const int bishop_magic_bitshifts[64] = {
    58, 59, 59, 59, 59, 59, 59, 58, 
    59, 59, 59, 59, 59, 59, 59, 59, 
    59, 59, 57, 57, 57, 57, 59, 59, 
    59, 59, 57, 55, 55, 57, 59, 59, 
    59, 59, 57, 55, 55, 57, 59, 59, 
    59, 59, 57, 57, 57, 57, 59, 59, 
    59, 59, 59, 59, 59, 59, 59, 59, 
    58, 59, 59, 59, 59, 59, 59, 58
};
u64 bishop_attacks[64][512];
u64 HashBishopOccConfig(int sq, u64 occ_config){
    return ((occ_config * bishop_magic_numbers[sq]) >> bishop_magic_bitshifts[sq]);
}

// ---

u64 rook_rays_no_edges[64];
u64 rook_rays_no_edges_occ_configs[64][512];

// ------

void PrecomputeKnightAttacks(){
    for(int sq = 0; sq < 64; sq++){
        u64 place = 1ULL << sq;

        // Eight squares read top --> bottom, left --> right on the board, so RIGHT bitshifts take you UP the board
        knight_attacks[sq] = (
            ((place & NOT_FILE_A) >> 17) | ((place & NOT_FILE_H) >> 15) |
            ((place & NOT_FILE_AB) >> 10) | ((place & NOT_FILE_GH) >> 6) |
            ((place & NOT_FILE_AB) << 6) | ((place & NOT_FILE_GH) << 10) |
            ((place & NOT_FILE_A) << 15) | ((place & NOT_FILE_H) << 17)
        );
    }
}

// ---

void PrecomputeBishopRaysNoEdges(){
    for(int sq = 0; sq < 64; sq++){
        u64 place = 1ULL << sq;
        bishop_rays_no_edges[sq] = 0ULL;
        int difference;

        // Up left
        difference = 9;
        while(!(place & RANK_8) && !(place & FILE_A) && !((place >> difference) & RANK_8) && !((place >> difference) & FILE_A)){
            bishop_rays_no_edges[sq] |= (place >> difference);
            difference += 9;
        }

        // Up right
        difference = 7;
        while(!(place & RANK_8) && !(place & FILE_H) && !((place >> difference) & RANK_8) && !((place >> difference) & FILE_H)){
            bishop_rays_no_edges[sq] |= (place >> difference);
            difference += 7;
        }

        // Down left
        difference = 7;
        while(!(place & RANK_1) && !(place & FILE_A) && !((place << difference) & RANK_1) && !((place << difference) & FILE_A)){
            bishop_rays_no_edges[sq] |= (place << difference);
            difference += 7;
        }

        // Down right
        difference = 9;
        while(!(place & RANK_1) && !(place & FILE_H) && !((place << difference) & RANK_1) && !((place << difference) & FILE_H)){
            //std::cout << sq << ": " << difference << "\n";
            bishop_rays_no_edges[sq] |= (place << difference);
            difference += 9;
        }
    }
}

void PrecomputeBishopRaysNoEdgesOccConfigs(){
    PrecomputeBishopRaysNoEdges();
    u64 mask;

    for(int sq = 0; sq < 64; sq++){

        for(int ci = 0; ci < 512; ci++){
            mask = bishop_rays_no_edges[sq];

            // Prepare the ci-th occupancy config
            u64 entry = 0;

            // Loop over the mask with an indexer
            int bit = 0;
            while(mask){
                int occ_square = GetLSBitIndex(mask);

                // This while-loop maps the bits of ci onto the bishop's rays (no edges). if the mask has less than 9 non-zero bits, there will be repetition
                if(GetBit(ci, bit)){ SetBit(entry, occ_square); }

                bit++;

                mask &= mask - 1;
            }

            // Testing purposes
            //std::bitset<64> x(entry);
            //std::cout << x << "\n";

            // Insert it
            bishop_rays_no_edges_occ_configs[sq][ci] = entry;
        }
    }
}

void PrecomputeBishopAttacksTable(){
    PrecomputeBishopRaysNoEdgesOccConfigs();

    for(int sq = 0; sq < 64; sq++){

        for(int ci = 0; ci < 512; ci++){
            u64 O = bishop_rays_no_edges_occ_configs[sq][ci];
            u64 hash = HashBishopOccConfig(sq, O);

            u64 place = 1ULL << sq;
            bishop_attacks[sq][hash] = 0ULL;
            int difference;

            // Up left
            difference = 9;
            while(!(place & RANK_8) && !(place & FILE_A)){
                place = place >> difference;
                bishop_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;

            // Up right
            difference = 7;
            while(!(place & RANK_8) && !(place & FILE_H)){
                place = place >> difference;
                bishop_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;

            // Down left
            difference = 7;
            while(!(place & RANK_1) && !(place & FILE_A)){
                place = place << difference;
                bishop_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;

            // Down right
            difference = 9;
            while(!(place & RANK_1) && !(place & FILE_H)){
                place = place << difference;
                bishop_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;
        }
    }
}

// ---

void PrecomputeRookRaysNoEdges(){
    for(int sq = 0; sq < 64; sq++){
        u64 place = 1ULL << sq;
        rook_rays_no_edges[sq] = 0ULL;
        int difference;

        // Up
        difference = 8;
        while(!(place & RANK_8) && !((place >> difference) & RANK_8)){
            rook_rays_no_edges[sq] |= (place >> difference);
            difference += 8;
        }

        // Left
        difference = 1;
        while(!(place & FILE_A) && !((place >> difference) & FILE_A)){
            rook_rays_no_edges[sq] |= (place >> difference);
            difference += 1;
        }

        // Right
        difference = 1;
        while(!(place & FILE_H) && !((place << difference) & FILE_H)){
            rook_rays_no_edges[sq] |= (place << difference);
            difference += 1;
        }

        // Down
        difference = 8;
        while(!(place & RANK_1) && !((place << difference) & RANK_1)){
            std::cout << sq << ": " << difference << "\n";
            rook_rays_no_edges[sq] |= (place << difference);
            difference += 8;
        }
    }
}

void PrecomputeRookRaysNoEdgesOccConfigs(){
    PrecomputeRookRaysNoEdges();

    //
}

void PrecomputeRookAttacksTable(){
    PrecomputeRookRaysNoEdgesOccConfigs();

    //
}