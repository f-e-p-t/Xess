import {
  GetPieceIndex, GetSquareIndex, PieceAtIndex, SetUpPromotionPanels, HandlePromotionMove,
  FillBoardFromBoardString, Until
} from "./utilities.js";

// Not using FEN notation - this is in a minimal form such that the board can be displayed on the screen
export let boardString = "----------------------------------------------------------------";

export const moveData = {
  source: 0,
  target: 0,
  // 0 - not a promo ||| 1 - knight ||| 2 - bishop ||| 3 - rook ||| 4 - queen
  pawnPromotionType: 0
};
let squareContainingDraggingMouse = 0;

let initialised = false;

async function InitialiseUI(){
  if(initialised){ return; }

  // Set the polling interval

  Homepage();
}

window.addEventListener("DOMContentLoaded", InitialiseUI, { once: true });

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

  // Fill it with pieces from boardString
  FillBoardFromBoardString();
}

Homepage();