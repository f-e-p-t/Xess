// Not using FEN notation - this is in a minimal form such that the board can be displayed on the screen
const boardString = "";

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
}

Homepage();