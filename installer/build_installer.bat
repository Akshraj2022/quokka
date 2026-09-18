@echo off
echo Building Quokka Installer...
python -m PyInstaller --onefile --windowed --noconsole --name "Quokka-Setup" --add-data "installer_assets\payload.zip;." --add-data "installer_assets\cfbl.png;." --add-data "installer_assets\txtcfbl.png;." --add-data "installer_assets\sticker.jpg;." --add-data "installer_assets\vscode_ext.zip;." --add-data "C:\\Users\\Acer\\AppData\\Local\\Programs\\Python\\Python311\\Lib\\site-packages\\customtkinter;customtkinter/" installer.py
echo Build complete. Look in the dist/ folder.
pause
