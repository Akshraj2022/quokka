@echo off
echo Building Quokka Installer...
python -m PyInstaller --onefile --windowed --noconsole --name "Quokka-Setup" --add-data "installer_assets\payload.zip;." --add-data "installer_assets\cfbl.png;." --add-data "installer_assets\txtcfbl.png;." --add-data "installer_assets\sticker.jpg;." --add-data "installer_assets\vscode_ext.zip;." installer.py
echo Build complete. Look in the dist/ folder.
pause
