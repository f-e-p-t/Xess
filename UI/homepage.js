import { ExtractNotFENFromBoard, GetPieceIndex, GetSquareIndex, PieceAtIndex } from "./utilities.js";

// Not using FEN notation - this is in a minimal form such that the board can be displayed on the screen
export let notFEN = "rnbqkbnrpppppppp--------------------------------PPPPPPPPRNBQKBNR";

let source = 0;
let target = 0;
// 0 - not a promo ||| 1 - knight ||| 2 - bishop ||| 3 - rook ||| 4 - queen ||| 5 - king
let pawnPromoType = 0;
let squareContainingDraggingMouse = 0;

function Homepage(){

  // Set up the empty board
  const board = document.querySelector(".board");
  let boardHTML = "";
  for(let i = 0; i < 8; i++){
    boardHTML += `
      <div class="board-row board-row-${i}">
    `;
    for(let j = 0; j < 8; j++){
      const squareIndex = (8 * i) + j;
      const bsb = "board-square-black"; const bsw = "board-square-white";
      boardHTML += `
        <div class="board-square board-square-${squareIndex} ${(i + j) % 2 ? bsb : bsw}">

        </div>
      `;
    }
    boardHTML += `
      </div>
    `;
  }
  board.innerHTML = boardHTML;

  // Fill it with pieces from notFEN
  for(let i = 0; i < 64; i++){
    const boardSquare = document.querySelector(`.board-square-${i}`);
    let boardSquareHTML = "";
    let pieceFileName = "";
    let pieceId = 0;
    switch(notFEN[i]){
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

  // Squares switch squareContainingDraggingMouse when drag mouse is dragged into them
  const squares = document.querySelectorAll(".board-square");
  squares.forEach((square) => {
    square.addEventListener("dragenter", () => {
      squareContainingDraggingMouse = GetSquareIndex(square);
    });
  });

  // Make the pieces drag-and-droppable
  const pieces = document.querySelectorAll(".piece-svg");
  pieces.forEach((piece) => {
    piece.addEventListener("dragstart", () => {
      piece.classList.add("see-through");
      source = GetPieceIndex(piece);
    });

    // On drag end is the move sent to the engine
    piece.addEventListener("dragend", () => {
      piece.classList.remove("see-through");
      target = squareContainingDraggingMouse;

      // Handle the extra data for pawn promotion moves
      if(0 <= target && target <= 7 && PieceAtIndex(source) === 1){ HandlePromoMove(); }
      else if(56 <= target && target <= 63 && PieceAtIndex(source) === 7){ HandlePromoMove(); }

      console.log(source, "to", target);
      console.log("piece:", PieceAtIndex(source));
    });
  })
}

Homepage();