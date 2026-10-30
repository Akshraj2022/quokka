@echo off
echo Building Quokka Installer...

REM Detect CustomTkinter path dynamically
for /f "delims=" %%i in ('python -c "import customtkinter, os; print(os.path.dirname(customtkinter.__file__))"') do set CTK_PATH=%%i

python -m PyInstaller --onefile --windowed --noconsole --name "Quokka-Setup" ^
    --add-data "installer_assets\payload.zip;." ^
    --add-data "installer_assets\cfbl.png;." ^
    --add-data "installer_assets\sticker.jpg;." ^
    --add-data "installer_assets\vscode_ext.zip;." ^
    --add-data "%CTK_PATH%;customtkinter/" installer.py

echo Build complete. Look in the dist/ folder.
pause
