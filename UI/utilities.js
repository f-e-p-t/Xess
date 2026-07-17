import { boardString } from "./homepage.js";

export function FillBoardFromBoardString(){
  for(let i = 0; i < 64; i++){
    const boardSquare = document.querySelector(`.board-square-${i}`);
    let boardSquareHTML = "";
    let pieceFileName = "";
    let pieceId = 0;
    switch(boardString[i]){
      case '-': { pieceFileName = ""; pieceId = 0; break; }
      case 'P': { pieceFileName = "pw"; pieceId = 1; break; }
      case 'N': { pieceFileName = "nw"; pieceId = 2; break; }
      case 'B': { pieceFileName = "bw"; pieceId = 3; break; }
      case 'R': { pieceFileName = "rw"; pieceId = 4; break; }
      case 'Q': { pieceFileName = "qw"; pieceId = 5; break; }
      case 'K': { pieceFileName = "kw"; pieceId = 6; break; }
      case 'p': { pieceFileName = "pb"; pieceId = 7; break; }
      case 'n': { pieceFileName = "nb"; pieceId = 8; break; }
      case 'b': { pieceFileName = "bb"; pieceId = 9; break; }
      case 'r': { pieceFileName = "rb"; pieceId = 10; break; }
      case 'q': { pieceFileName = "qb"; pieceId = 11; break; }
      case 'k': { pieceFileName = "kb"; pieceId = 12; break; }
      default: { break; }
    }
    boardSquareHTML += `<div class="piece-container piece-container-${i} contains-piece-${pieceId}">`;
    if(pieceFileName){
      boardSquareHTML += `
        <img src="./Assets/${pieceFileName}.svg" height="65" width="65" class="piece-svg piece-svg-${i}">
      `;
    }
    boardSquareHTML += `</div>`;
    boardSquare.innerHTML = boardSquareHTML;
  }
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

export function PieceAtIndex(index){
  const squareWithIndex = document.querySelector(`.piece-container-${index}`);
  for(let piece = 0; piece <= 12; piece++){
    if(squareWithIndex.classList.contains(`contains-piece-${piece}`)){ return piece; }
  }
}