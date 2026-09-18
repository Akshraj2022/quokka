import sys
import os
import zipfile
import shutil
import ctypes
import threading
import time
import winreg
import random
from tkinter import *
from tkinter import ttk, messagebox, filedialog
from PIL import Image, ImageTk

# Setup Paths
def resource_path(relative_path):
    if hasattr(sys, '_MEIPASS'):
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.abspath("installer_assets"), relative_path)

def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

QUOKKA_FACTS = [
    "Fun Fact: Quokkas are known as the world's happiest animals due to their iconic smiles!",
    "Fun Fact: Quokkas have virtually no fear of humans.",
    "Fun Fact: They are small macropods, making them relatives of kangaroos and wallabies.",
    "Fun Fact: Quokkas store fat in their tails to survive scarce seasons.",
    "Fun Fact: They can survive a month without drinking water by eating succulents.",
    "Fun Fact: Quokkas are mostly nocturnal, foraging at night.",
    "Fun Fact: Female quokkas carry their babies (joeys) in a pouch for six months."
]

class QuokkaInstaller(Tk):
    def __init__(self):
        super().__init__()
        self.title("Quokka Setup")
        self.geometry("600x420")
        self.configure(bg="#1E1E1E")
        self.resizable(False, False)
        
        # Styles
        style = ttk.Style()
        style.theme_use('clam')
        style.configure("TFrame", background="#1E1E1E")
        style.configure("TLabel", background="#1E1E1E", foreground="#FFFFFF", font=("Segoe UI", 10))
        style.configure("TButton", background="#333333", foreground="#FFFFFF", font=("Segoe UI", 10))
        style.configure("TCheckbutton", background="#1E1E1E", foreground="#FFFFFF", font=("Segoe UI", 10))
        style.configure("Horizontal.TProgressbar", background="#007ACC", troughcolor="#333333")
        
        self.pages = {}
        self.current_page = None
        
        # Load Assets
        try:
            self.img_logo = ImageTk.PhotoImage(Image.open(resource_path("cfbl.png")).resize((120, 120)))
            self.img_txtlogo = ImageTk.PhotoImage(Image.open(resource_path("txtcfbl.png")).resize((200, 50)))
            self.img_sticker = ImageTk.PhotoImage(Image.open(resource_path("sticker.jpg")).resize((150, 150)))
        except Exception as e:
            print("Error loading images:", e)
            self.img_logo = None
            self.img_txtlogo = None
            self.img_sticker = None
            
        self.setup_ui()
        
    def setup_ui(self):
        self.container = Frame(self, bg="#1E1E1E")
        self.container.pack(fill=BOTH, expand=True)
        
        # Define Pages
        self.pages["welcome"] = self.create_welcome_page()
        self.pages["options"] = self.create_options_page()
        self.pages["progress"] = self.create_progress_page()
        self.pages["finish"] = self.create_finish_page()
        
        self.show_page("welcome")

    def show_page(self, name):
        if self.current_page:
            self.current_page.pack_forget()
        self.current_page = self.pages[name]
        self.current_page.pack(fill=BOTH, expand=True)

    def create_welcome_page(self):
        frame = ttk.Frame(self.container)
        
        ttk.Label(frame, text="", font=("Segoe UI", 20)).pack(pady=10)
        
        if self.img_logo: ttk.Label(frame, image=self.img_logo).pack(pady=20)
        
        ttk.Label(frame, text="Quokka", font=("Segoe UI Light", 28, "bold"), foreground="#007ACC").pack()
        ttk.Label(frame, text="Version 1.0.0 Setup", font=("Segoe UI", 12, "italic"), foreground="#888888").pack(pady=5)
        
        btn_frame = ttk.Frame(frame)
        btn_frame.pack(side=BOTTOM, fill=X, pady=20, padx=20)
        ttk.Button(btn_frame, text="Next >", command=lambda: self.show_page("options")).pack(side=RIGHT)
        ttk.Button(btn_frame, text="Cancel", command=self.destroy).pack(side=RIGHT, padx=10)
        
        return frame

    def create_options_page(self):
        frame = ttk.Frame(self.container)
        ttk.Label(frame, text="Installation Options", font=("Segoe UI", 16, "bold")).pack(pady=20)
        
        # Path selection
        path_frame = ttk.Frame(frame)
        path_frame.pack(fill=X, padx=40, pady=10)
        ttk.Label(path_frame, text="Install Location:").pack(side=LEFT)
        
        default_path = os.path.join(os.environ.get("LOCALAPPDATA", "C:\\"), "Programs", "Quokka")
        if is_admin():
            default_path = os.path.join(os.environ.get("PROGRAMFILES", "C:\\Program Files"), "Quokka")
            
        self.path_var = StringVar(value=default_path)
        ttk.Entry(path_frame, textvariable=self.path_var, width=40).pack(side=LEFT, padx=10)
        ttk.Button(path_frame, text="Browse...", command=self.browse_path).pack(side=LEFT)
        
        # Components
        comp_frame = ttk.Frame(frame)
        comp_frame.pack(fill=X, padx=40, pady=10)
        
        self.var_path = BooleanVar(value=True)
        self.var_assoc = BooleanVar(value=True)
        self.var_joey = BooleanVar(value=True)
        
        ttk.Checkbutton(comp_frame, text="Add Quokka to PATH", variable=self.var_path).pack(anchor=W)
        ttk.Checkbutton(comp_frame, text="Associate .qk files with Quokka", variable=self.var_assoc).pack(anchor=W)
        ttk.Checkbutton(comp_frame, text="Install Joey ML Extension", variable=self.var_joey).pack(anchor=W)
        
        btn_frame = ttk.Frame(frame)
        btn_frame.pack(side=BOTTOM, fill=X, pady=20, padx=20)
        ttk.Button(btn_frame, text="Install", command=self.start_install).pack(side=RIGHT)
        ttk.Button(btn_frame, text="< Back", command=lambda: self.show_page("welcome")).pack(side=RIGHT, padx=10)
        return frame

    def browse_path(self):
        d = filedialog.askdirectory(initialdir=self.path_var.get())
        if d: self.path_var.set(os.path.normpath(d))

    def create_progress_page(self):
        frame = ttk.Frame(self.container)
        ttk.Label(frame, text="Installing Quokka...", font=("Segoe UI", 16, "bold")).pack(pady=20)
        if self.img_sticker: ttk.Label(frame, image=self.img_sticker).pack(pady=10)
        
        self.progress_lbl = ttk.Label(frame, text="Preparing...")
        self.progress_lbl.pack(pady=5)
        self.progress_bar = ttk.Progressbar(frame, orient=HORIZONTAL, length=400, mode='determinate')
        self.progress_bar.pack(pady=5)
        
        self.fact_lbl = ttk.Label(frame, text=random.choice(QUOKKA_FACTS), font=("Segoe UI", 9, "italic"), foreground="#007ACC")
        self.fact_lbl.pack(pady=15)
        
        return frame
        
    def create_finish_page(self):
        frame = ttk.Frame(self.container)
        ttk.Label(frame, text="Installation Complete!", font=("Segoe UI", 16, "bold")).pack(pady=20)
        if self.img_logo: ttk.Label(frame, image=self.img_logo).pack(pady=10)
        
        self.finish_lbl = ttk.Label(frame, text="Quokka has been installed successfully.")
        self.finish_lbl.pack(pady=10)
        
        self.finish_fact = ttk.Label(frame, text=random.choice(QUOKKA_FACTS), font=("Segoe UI", 9, "italic"), foreground="#007ACC")
        self.finish_fact.pack(pady=10)
        
        btn_frame = ttk.Frame(frame)
        btn_frame.pack(side=BOTTOM, fill=X, pady=20, padx=20)
        ttk.Button(btn_frame, text="Finish", command=self.destroy).pack(side=RIGHT)
        return frame

    def start_install(self):
        self.show_page("progress")
        threading.Thread(target=self.install_process, daemon=True).start()

    def update_progress(self, val, text):
        self.progress_bar['value'] = val
        self.progress_lbl.config(text=text)
        if random.random() > 0.6:  # Occasionally rotate facts during updates
            self.fact_lbl.config(text=random.choice(QUOKKA_FACTS))
        self.update()

    def install_process(self):
        target_dir = self.path_var.get()
        try:
            os.makedirs(target_dir, exist_ok=True)
            self.update_progress(10, "Extracting Quokka core...")
            
            # Extract payload
            payload_path = resource_path("payload.zip")
            if os.path.exists(payload_path):
                with zipfile.ZipFile(payload_path, 'r') as zf:
                    zf.extractall(target_dir)
            
            time.sleep(1.0)
            self.update_progress(50, "Configuring components...")
            
            # Create uninstaller script
            uninst_path = os.path.join(target_dir, "uninstall.bat")
            with open(uninst_path, "w") as f:
                f.write(f'@echo off\n')
                f.write(f'echo Uninstalling Quokka...\n')
                f.write(f'rmdir /s /q "{target_dir}"\n')
                if self.var_path.get():
                    f.write(f'echo Please remove Quokka from your PATH manually.\n')
                f.write(f'pause\n')
                
            time.sleep(1.0)
            if self.var_path.get():
                self.update_progress(70, "Updating PATH...")
                self.add_to_path(target_dir)
                
            time.sleep(1.0)
            if self.var_assoc.get():
                self.update_progress(80, "Associating .qk files and adding Context Menus...")
                self.associate_files(target_dir)
                
                self.update_progress(90, "Installing VS Code Extension...")
                self.install_vscode_ext()
                
            self.update_progress(100, "Finishing up...")
            time.sleep(1.0)
            self.after(0, lambda: self.show_page("finish"))
            
        except Exception as e:
            self.after(0, lambda: messagebox.showerror("Installation Error", f"Quokka couldn't install this component because an error occurred:\n{e}"))
            self.after(0, lambda: self.show_page("options"))

    def add_to_path(self, target_dir):
        try:
            key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Environment", 0, winreg.KEY_ALL_ACCESS)
            try:
                path, _ = winreg.QueryValueEx(key, "Path")
            except WindowsError:
                path = ""
            if target_dir not in path:
                new_path = path + ";" + target_dir if path else target_dir
                winreg.SetValueEx(key, "Path", 0, winreg.REG_EXPAND_SZ, new_path)
                import ctypes
                HWND_BROADCAST = 0xFFFF
                WM_SETTINGCHANGE = 0x001A
                ctypes.windll.user32.SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, "Environment", 2, 1000, None)
            winreg.CloseKey(key)
        except Exception as e:
            print("Failed to add to PATH:", e)

    def install_vscode_ext(self):
        try:
            vscode_ext_dir = os.path.join(os.environ.get("USERPROFILE", ""), ".vscode", "extensions", "quokka-lang-1.0.0")
            os.makedirs(vscode_ext_dir, exist_ok=True)
            ext_zip = resource_path("vscode_ext.zip")
            if os.path.exists(ext_zip):
                with zipfile.ZipFile(ext_zip, 'r') as zf:
                    # Depending on how it zipped, extract properly.
                    # Compress-Archive creates the folder inside the zip.
                    zf.extractall(vscode_ext_dir)
        except Exception as e:
            print("Failed to install VS Code extension:", e)

    def associate_files(self, target_dir):
        try:
            qk_exe = os.path.join(target_dir, "quokka.bat")
            key_ext = winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Classes\.qk")
            winreg.SetValue(key_ext, "", winreg.REG_SZ, "QuokkaScript")
            winreg.CloseKey(key_ext)
            
            key_cls = winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Classes\QuokkaScript")
            winreg.SetValue(key_cls, "", winreg.REG_SZ, "Quokka Script File")
            
            # Run action
            key_cmd = winreg.CreateKey(key_cls, r"shell\open\command")
            winreg.SetValue(key_cmd, "", winreg.REG_SZ, f'"{qk_exe}" run "%1"')
            winreg.CloseKey(key_cmd)
            
            # Edit in Notepad
            key_edit_np = winreg.CreateKey(key_cls, r"shell\Edit in Notepad\command")
            winreg.SetValue(key_edit_np, "", winreg.REG_SZ, 'notepad.exe "%1"')
            winreg.CloseKey(key_edit_np)
            
            # Edit in VS Code
            key_edit_vs = winreg.CreateKey(key_cls, r"shell\Edit in VS Code\command")
            winreg.SetValue(key_edit_vs, "", winreg.REG_SZ, 'cmd.exe /c code "%1"')
            winreg.CloseKey(key_edit_vs)
            
            winreg.CloseKey(key_cls)
        except Exception as e:
            print("Failed to associate .qk files:", e)

if __name__ == "__main__":
    app = QuokkaInstaller()
    app.mainloop()
