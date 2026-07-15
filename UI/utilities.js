import { notFEN } from "./homepage.js";

export function ExtractNotFENFromBoard(){
  
}

export function GetSquareIndex(square){
  for(let i = 0; i < 64; i++){
    if(square.classList.contains(`board-square-${i}`)){
      return i;
    }
  }
}

export function GetPieceIndex(piece){
  for(let i = 0; i < 64; i++){
    if(piece.classList.contains(`piece-svg-${i}`)){
      return i;
    }
  }
}

export function HandlePromoMove(){
  // handle it
}

export function PieceAtIndex(index){
  const squareWithIndex = document.querySelector(`.piece-container-${index}`);
  for(let piece = 0; piece <= 12; piece++){
    if(squareWithIndex.classList.contains(`contains-piece-${piece}`)){ return piece; }
  }
}