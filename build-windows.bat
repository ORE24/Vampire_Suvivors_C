@echo off
setlocal

if not exist build mkdir build

gcc -std=c99 -Wall -Wextra -O2 ^
  src\entities.c ^
  src\game.c ^
  src\main.c ^
  src\platform.c ^
  src\ui.c ^
  src\weapons.c ^
  -o build\vampire-survivors-c.exe ^
  -lm

if errorlevel 1 exit /b %errorlevel%

echo Built build\vampire-survivors-c.exe
