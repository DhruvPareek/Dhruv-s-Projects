#include "Game.h"
#include "globals.h"
#include "Board.h"
#include "Player.h"
#include <iostream>
#include <string>

using namespace std;

bool addStandardShips(Game& g)
{
    return g.addShip(5, 'A', "aircraft carrier")  &&
    g.addShip(4, 'B', "battleship")  &&
    g.addShip(3, 'D', "destroyer")  &&
    g.addShip(3, 'S', "submarine")  &&
    g.addShip(2, 'P', "patrol boat");
}

int main()
{


    
    
    /**
    int goodWins = 0;
    for(int i = 0; i < 1000; i++)
    {
        
        Game g(10, 10);
   
        Board b(g);
        addStandardShips(g);
    
        Player* p1 = createPlayer("mediocre", "mediocre", g);
        Player* p2 = createPlayer("good", "goodplayer", g);
        if(g.play(p1, p2, false) == p2)
            goodWins++;
    }
    
    cout << endl << endl << goodWins;
     **/
    

        Game g(10, 10);
   
        Board b(g);
        addStandardShips(g);
    
        Player* p1 = createPlayer("mediocre", "mediocre", g);
        Player* p2 = createPlayer("human", "human", g);
        g.play(p2, p1, true);
 

}
