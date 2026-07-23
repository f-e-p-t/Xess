import {
  FillBoardFromBoardString, GetPieceIndex, GetSquareIndex, PieceAtIndex,
  UpdateSourceAndTargetHighlights
} from "./utilities.js";

// Not using FEN notation - this is in a minimal form such that the board can be displayed on the screen
export let boardString = "----------------------------------------------------------------";

let initialised = false;
let lastMoveInfo = {
  source: -1,
  target: -1
};

async function InitialiseUI(){
  if(initialised){ return; }

  setInterval(PollBoardStringFromEngine, 400);

  Homepage();
}

window.addEventListener("DOMContentLoaded", InitialiseUI, { once: true });

async function PollBoardStringFromEngine(){
  const response = await fetch("http://localhost:8000/poll-board-update");
  const responseBody = await response.json();

  if(!response.ok){
    console.log("Error receiving board update");
    return;
  }

  console.log(responseBody);
  boardString = responseBody.board;
  lastMoveInfo.source = responseBody.source;
  lastMoveInfo.target = responseBody.target;

  Homepage();
}

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

  // Update the highlighted source and target squares
  UpdateSourceAndTargetHighlights(lastMoveInfo.source, lastMoveInfo.target);
}

Homepage();