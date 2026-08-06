@echo off
rem Launches the built application with the Qt runtime on PATH.
set PATH=C:\Qt\6.11.1\mingw_64\bin;%PATH%
start "" "%~dp0build\LibraryApp.exe"
