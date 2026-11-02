AGS Starter for Mac
===================

A retro launcher for AGS games (Adventure Game Studio) on the Mac.
The AGS engine is built in.

Install
  1. Drag "AGS Starter" into your Applications folder.
  2. First launch: macOS blocks apps downloaded from the internet.
     Open Terminal and type (note the space at the end):
        xattr -cr 
     then drag "AGS Starter" from Finder into Terminal and press Enter.
  3. On first start, choose your games folder.
     Put each game in its own subfolder, e.g.
        Games/Aftergold/  (Aftergold.ags, audio.vox, ...)

Buttons
  PLAY         start the selected game
  OPEN FOLDER  show your games folder in Finder (to add games)
  RESCAN       look for new games in the folder
  SETTINGS     choose the games folder / update the AGS engine
  ABOUT        what AGS is, engine version, controls
  QUIT         close AGS Starter

Controls
  - Click a game to select it, then PLAY
  - Double-click or Enter plays right away
  - Buttons act when you release the mouse – drag away to cancel
  - Arrow keys / mouse wheel: scroll · Esc: quit · F5: rescan
  - Make the window bigger to zoom the whole screen

Microphone prompt when a game starts: choose "Don't Allow" – it is not needed.
Log file for troubleshooting: ~/Library/Logs/AGS Starter.log

AGS (Adventure Game Studio) is open source: https://github.com/adventuregamestudio/ags
Made by MENRIX (Don_Key_Moustache) – MIT License.
Third-party parts (AGS engine, SDL2, fonts): see THIRD-PARTY-NOTICES.md.
