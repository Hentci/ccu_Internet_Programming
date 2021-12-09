#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int winner = 0;
char board[3][3];
void welcome(){
    printf("==================================\n");
    printf("[Welcome to the |TIC-TAC-TOE| !!!]\n");
    printf("==================================\n");
}
void printBoard(char board[][3]){
    printf(" =================\n");
    printf("[     |     |     ] \n");
    printf("[  %c  |  %c  |  %c  |\n", board[0][0], board[0][1], board[0][2]);
    printf("[=====|=====|=====]\n");
    printf("[     |     |     ]\n");
    printf("[  %c  |  %c  |  %c  |\n", board[1][0], board[1][1], board[1][2]);
    printf("[=====|=====|=====]\n");
    printf("[     |     |     ]\n");
    printf("[  %c  |  %c  |  %c  |\n", board[2][0], board[2][1], board[2][2]);
    printf("[=====|=====|=====]\n");
}

void check(char board[][3]){
    int i;
    char win = ' ';

    // rows
    for (i = 0; i < 3; i++)
        if (board[i][0] == board[i][1] && board[i][0] == board[i][2] && board[i][0] != ' ')
            win = board[i][0];
    // columns
    for (i = 0; i < 3; i++)
        if (board[0][i] == board[1][i] && board[0][i] == board[2][i] && board[0][i] != ' ')
            win = board[0][i];
    // diagonals
    if (board[0][0] == board[1][1] && board[1][1] == board[2][2] && board[1][1] != ' ')
        win = board[1][1];
    if (board[0][2] == board[1][1] && board[1][1] == board[2][0] && board[1][1] != ' ')
        win = board[1][1];

    if (win == 'X'){
        printBoard(board);
        printf("Player 1 Wins\n");
        winner = 1;
        exit(0);
    }

    if (win == 'O'){
        printBoard(board);
        printf("Player 2 Wins\n	");
        winner = 2;
        exit(0);
    }
}

int main(){
    int player = 0;
    int winner = 0;
    int choice = 0, row = 0, column = 0, line = 0, i = 0;
    char numberBoard[3][3] = {// to display positions to choose from
                              {'1', '2', '3'},
                              {'4', '5', '6'},
                              {'7', '8', '9'}};
    char board[3][3] = {// to display the actual game status
                        {' ', ' ', ' '},
                        {' ', ' ', ' '},
                        {' ', ' ', ' '}};

    welcome();

    for (i = 0; i < 9 && winner == 0; i++){
        printBoard(numberBoard);
        printf("~~~~~~~~~~~~~~~~~~~~\n");
        printf("~The game continues~\n");
        printf("~~~~~~~~~~~~~~~~~~~~\n");
        printBoard(board);
        player = i % 2 + 1;

        while(1){
            printf("\nPlayer %d, Please enter the number of the position where you want to place your %c: ", player, (player == 1) ? 'X' : 'O');
            scanf("%d", &choice);

            row = --choice / 3;
            column = choice % 3;

            if (choice < 0 || choice > 9 || board[row][column] > '9' || board[row][column] == 'X' || board[row][column] == 'O')
                printf("Invalid Input. Please Enter again.\n");
            else break;
        }
        board[row][column] = (player == 1) ? 'X' : 'O';
        check(board); 
    }
}
