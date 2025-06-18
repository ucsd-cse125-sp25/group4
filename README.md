# Tiny Terrors
## by Visual Studios

Tiny Terrors a game built in UCSD's CSE 125 Software System Design and Implementation course.

The game is a 3v1 game, where 3 runners try to escape a hunter, a cursed evil doll, over multiple rounds. 
At the end of each round, players can purchase powerups that will modify the game and grant abilies.

The hunter is automatically assigned as the first player that joins.

## Download

Pull and open `GameNetworkingDemo.sln` in Visual Studio, and build.
Move `[repo]\FMOD_API\core\lib\x64\fmod.dll`, `[repo]\FMOD_API\core\lib\x64\fmodL.dll`, `[repo]\FMOD_API\studio\lib\x64\fmodstudio.dll`, `[repo]\FMOD_API\studio\lib\x64\fmodstudioL.dll` into `[repo]\x64\Debug\`. (you might need to move `\Assets\textures\` into this directory as well.)

After everything is built, launch the server using `GameServer.exe` and up to 4 clients using `ClientApp.exe`. 

## Controls

Movement - `WASD`  
Jump - `[Space]`  
Attack - `Left click`  
Dodge - `Right click`  
Nocturnal Powerup - `R`  
Phantom Powerup - `E`  
Bear Powerup - `E` in vicinity of the Bear  
Ready/Purchase - `[Enter]`
Select powerup for purchase - `[1] [2] [3]`


## Visual Studios (Group 4) Members

Jaiden Ekgasit  
Jerry Gong  
Jiwen (Will) Luo  
Chase Peterson   
Alexander Tahan  
Andrew Yang   
John Zhou
