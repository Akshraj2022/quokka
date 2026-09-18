#!/bin/bash
echo "Building Quokka Installer for Unix (Mac/Linux)..."

# Ensure PyInstaller and customtkinter are installed
pip3 install pyinstaller customtkinter pillow > /dev/null 2>&1

# Find customtkinter path dynamically
CTK_PATH=$(python3 -c "import customtkinter, os; print(os.path.dirname(customtkinter.__file__))")

# On Unix, the separator for --add-data is : instead of ;
python3 -m PyInstaller --onefile --windowed --noconsole --name "Quokka-Setup" \
    --add-data "installer_assets/payload.zip:." \
    --add-data "installer_assets/cfbl.png:." \
    --add-data "installer_assets/sticker.jpg:." \
    --add-data "installer_assets/vscode_ext.zip:." \
    --add-data "$CTK_PATH:customtkinter/" installer.py

echo "Build complete. Look in the dist/ folder."

