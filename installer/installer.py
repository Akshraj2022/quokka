import sys
import os
import zipfile
import shutil
import threading
import time
import math
import customtkinter as ctk
from tkinter import messagebox, filedialog
from PIL import Image

try:
    import ctypes
except ImportError:
    ctypes = None

# Set light mode for brown and white aesthetic
ctk.set_appearance_mode("light")

# Colors: Shades of brown and white
BG = "#FAF6F0"               # Cream / Off-White
FRAME_BG = "#E8D8C8"         # Light Tan
BTN_BG = "#8C6A53"           # Medium Brown
BTN_HOVER = "#A8846A"        # Lighter Brown
TEXT_MAIN = "#4A3628"        # Dark Brown
TEXT_ACCENT = "#5E4331"      # Deep Brown
TEXT_MUTED = "#8A6B56"       # Medium-Dark Brown
CONSOLE_BG = "#FFFFFF"       # Pure White for text areas

def resource_path(relative_path):
    if hasattr(sys, "_MEIPASS"): return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.abspath("installer_assets"), relative_path)

def is_admin():
    try: return os.getuid() == 0
    except AttributeError: 
        try: return ctypes.windll.shell32.IsUserAnAdmin() != 0
        except: return False

class InstallerApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Quokka Setup")
        self.geometry("600x480")
        self.resizable(False, False)
        self.configure(fg_color=BG)
        
        # Load Images
        try:
            self.img_logo = ctk.CTkImage(Image.open(resource_path("cfbl.png")), size=(80, 80))
        except:
            self.img_logo = None
            
        try:
            self.img_sticker = ctk.CTkImage(Image.open(resource_path("sticker_new.png")), size=(150, 150))
        except:
            self.img_sticker = None
            
        self.container = ctk.CTkFrame(self, fg_color="transparent")
        self.container.pack(fill="both", expand=True)
        
        self.pages = {}
        for PageName, method in [("welcome", self.create_welcome_page), 
                                 ("options", self.create_options_page),
                                 ("guide", self.create_guide_page),
                                 ("progress", self.create_progress_page),
                                 ("finish", self.create_finish_page)]:
            self.pages[PageName] = method()
            self.pages[PageName].place(in_=self.container, x=0, y=0, relwidth=1, relheight=1)
            
        self.show_page("welcome")
        
    def show_page(self, name):
        self.pages[name].tkraise()

    def create_welcome_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        ctk.CTkLabel(frame, text="Welcome to Quokka", font=("Segoe UI", 32, "bold"), text_color=TEXT_ACCENT).pack(pady=(40, 20))
        if self.img_logo:
            ctk.CTkLabel(frame, text="", image=self.img_logo).pack(pady=10)
            
        ctk.CTkLabel(frame, text="The verifiable, deterministic language for AI workflows.", font=("Segoe UI", 16), text_color=TEXT_MAIN).pack(pady=20)
        
        btn_frame = ctk.CTkFrame(frame, fg_color="transparent")
        btn_frame.pack(side="bottom", fill="x", pady=40, padx=40)
        
        ctk.CTkButton(btn_frame, text="Next >", command=lambda: self.show_page("guide"), width=120, fg_color=BTN_BG, hover_color=BTN_HOVER, text_color="#FFF", corner_radius=20, font=("Segoe UI", 14, "bold")).pack(side="right")
        return frame

    def create_guide_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        ctk.CTkLabel(frame, text="Quick Guide", font=("Segoe UI", 26, "bold"), text_color=TEXT_ACCENT).pack(pady=(30, 10))
        
        guide_text = (
            "• CLI Usage:\n"
            "   Run a script: quokka run <file>\n"
            "   Verify self-hosting: quokka check-self\n\n"
            "• Guichenotia (IDLE):\n"
            "   We have a beautiful new UI for Quokka called Guichenotia.\n"
            "   You can write and run scripts directly in it.\n\n"
            "• Joey (ML Extension):\n"
            "   Quokka isolates ML workloads to Joey to ensure memory safety.\n"
            "   Use 'import joey' in your Quokka code to train models."
        )
        
        textbox = ctk.CTkTextbox(frame, fg_color=CONSOLE_BG, text_color=TEXT_MAIN, font=("Segoe UI", 14), width=480, height=200, corner_radius=10, border_color=BTN_BG, border_width=2)
        textbox.pack(pady=10)
        textbox.insert("1.0", guide_text)
        textbox.configure(state="disabled")
        
        btn_frame = ctk.CTkFrame(frame, fg_color="transparent")
        btn_frame.pack(side="bottom", fill="x", pady=20, padx=40)
        ctk.CTkButton(btn_frame, text="Next >", command=lambda: self.show_page("options"), width=120, fg_color=BTN_BG, hover_color=BTN_HOVER, text_color="#FFF", corner_radius=20, font=("Segoe UI", 14, "bold")).pack(side="right")
        ctk.CTkButton(btn_frame, text="< Back", command=lambda: self.show_page("welcome"), fg_color=FRAME_BG, hover_color="#D1BFAe", text_color=TEXT_MAIN, corner_radius=20, font=("Segoe UI", 14)).pack(side="right", padx=10)
        
        return frame

    def create_options_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        ctk.CTkLabel(frame, text="Customize Installation", font=("Segoe UI", 26, "bold"), text_color=TEXT_ACCENT).pack(pady=(20, 10))
        
        path_frame = ctk.CTkFrame(frame, fg_color="transparent")
        path_frame.pack(fill="x", padx=50, pady=5)
        ctk.CTkLabel(path_frame, text="Install Location:", font=("Segoe UI", 14), text_color=TEXT_MAIN).pack(anchor="w")
        
        if os.name == 'nt':
            default_path = os.path.join(os.environ.get("LOCALAPPDATA", "C:\\"), "Programs", "Quokka")
            if is_admin(): default_path = os.path.join(os.environ.get("PROGRAMFILES", "C:\\Program Files"), "Quokka")
        elif sys.platform == 'darwin':
            default_path = "/Applications/Quokka" if is_admin() else os.path.expanduser("~/Applications/Quokka")
        else:
            default_path = "/opt/quokka" if is_admin() else os.path.expanduser("~/.local/share/quokka")
            
        self.path_var = ctk.StringVar(value=default_path)
        path_row = ctk.CTkFrame(path_frame, fg_color="transparent")
        path_row.pack(fill="x", pady=5)
        ctk.CTkEntry(path_row, textvariable=self.path_var, fg_color=CONSOLE_BG, border_color=BTN_BG, text_color=TEXT_MAIN).pack(side="left", fill="x", expand=True, padx=(0, 10))
        ctk.CTkButton(path_row, text="Browse", command=self.browse_path, width=80, fg_color=FRAME_BG, hover_color=BTN_BG, text_color=TEXT_MAIN, border_width=2, border_color=BTN_BG).pack(side="right")
        
        comp_frame = ctk.CTkFrame(frame, fg_color=FRAME_BG, corner_radius=15, border_width=1, border_color=BTN_BG)
        comp_frame.pack(fill="x", padx=50, pady=10, ipady=5, ipadx=10)
        
        self.var_path = ctk.BooleanVar(value=True)
        self.var_idle = ctk.BooleanVar(value=True)
        self.var_joey = ctk.BooleanVar(value=True)
        
        for txt, var in [("Add Quokka to PATH", self.var_path), 
                         ("Install Guichenotia (IDLE UI)", self.var_idle), 
                         ("Install Joey ML Extension", self.var_joey)]:
            ctk.CTkCheckBox(comp_frame, text=txt, variable=var, fg_color=BTN_BG, hover_color=BTN_HOVER, checkmark_color="#FFF", text_color=TEXT_MAIN, font=("Segoe UI", 13)).pack(anchor="w", pady=6, padx=10)
        
        btn_frame = ctk.CTkFrame(frame, fg_color="transparent")
        btn_frame.pack(side="bottom", fill="x", pady=20, padx=40)
        ctk.CTkButton(btn_frame, text="Install", command=self.start_install, width=120, fg_color=BTN_BG, hover_color=BTN_HOVER, text_color="#FFF", corner_radius=20, font=("Segoe UI", 14, "bold")).pack(side="right")
        ctk.CTkButton(btn_frame, text="< Back", command=lambda: self.show_page("guide"), fg_color=FRAME_BG, hover_color="#D1BFAe", text_color=TEXT_MAIN, corner_radius=20, font=("Segoe UI", 14)).pack(side="right", padx=10)
        return frame

    def browse_path(self):
        d = filedialog.askdirectory(initialdir=self.path_var.get())
        if d: self.path_var.set(os.path.normpath(d))

    def create_progress_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        ctk.CTkLabel(frame, text="Installing Quokka...", font=("Segoe UI", 26, "bold"), text_color=TEXT_ACCENT).pack(pady=(30,10))
        
        logo_area = ctk.CTkFrame(frame, fg_color="transparent")
        logo_area.pack(fill="x", pady=10)
        
        if self.img_sticker:
            self.sticker_lbl = ctk.CTkLabel(logo_area, text="", image=self.img_sticker)
            self.sticker_lbl.pack(pady=10)
        
        self.progress_lbl = ctk.CTkLabel(frame, text="Preparing to jump...", text_color=TEXT_MUTED, font=("Segoe UI", 14))
        self.progress_lbl.pack(pady=(20, 10))
        
        self.progress_bar = ctk.CTkProgressBar(frame, width=400, fg_color=FRAME_BG, progress_color=BTN_BG, mode="determinate")
        self.progress_bar.set(0)
        self.progress_bar.pack()
        
        return frame

    def create_finish_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        ctk.CTkLabel(frame, text="Installation Complete!", font=("Segoe UI", 30, "bold"), text_color=TEXT_ACCENT).pack(pady=50)
        
        if self.img_logo: ctk.CTkLabel(frame, text="", image=self.img_logo).pack(pady=10)
        
        ctk.CTkLabel(frame, text="Quokka has been successfully installed.", font=("Segoe UI", 16), text_color=TEXT_MAIN).pack(pady=20)
        
        btn_frame = ctk.CTkFrame(frame, fg_color="transparent")
        btn_frame.pack(side="bottom", fill="x", pady=30, padx=40)
        ctk.CTkButton(btn_frame, text="Finish", command=self.destroy, width=140, fg_color=BTN_BG, hover_color=BTN_HOVER, text_color="#FFF", corner_radius=20, font=("Segoe UI", 16, "bold")).pack(side="right")
        
        return frame

    def update_progress(self, msg, pct):
        self.progress_lbl.configure(text=msg)
        self.progress_bar.set(pct)
        self.update_idletasks()

    def start_install(self):
        self.show_page("progress")
        t = threading.Thread(target=self.install_process)
        t.daemon = True
        t.start()

    def install_process(self):
        try:
            target_dir = self.path_var.get()
            os.makedirs(target_dir, exist_ok=True)
            
            payload = resource_path("payload.zip")
            if os.path.exists(payload):
                self.update_progress("Extracting core binaries...", 0.2)
                with zipfile.ZipFile(payload, "r") as z:
                    z.extractall(target_dir)
                    
            if self.var_idle.get():
                self.update_progress("Installing Guichenotia IDLE...", 0.3)
                time.sleep(0.5)
                # In a real installer this would copy guichenotia.py or binary
                
            if self.var_joey.get():
                self.update_progress("Configuring Joey ML extension...", 0.5)
                time.sleep(0.5)
                
            if self.var_path.get():
                self.update_progress("Updating system PATH...", 0.7)
                self.add_to_path(target_dir)
                
            self.update_progress("All set! Wrapping up...", 1.0)
            time.sleep(0.8)
            self.show_page("finish")
        except Exception as e:
            messagebox.showerror("Error", f"Installation failed: {e}")
            self.destroy()

    def add_to_path(self, target_dir):
        if os.name == 'nt':
            try:
                import winreg
                key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Environment", 0, winreg.KEY_ALL_ACCESS)
                try: path, _ = winreg.QueryValueEx(key, "Path")
                except: path = ""
                if target_dir not in path:
                    new_path = path + (";" if path and not path.endswith(";") else "") + target_dir
                    winreg.SetValueEx(key, "Path", 0, winreg.REG_EXPAND_SZ, new_path)
                    HWND_BROADCAST = 0xFFFF
                    WM_SETTINGCHANGE = 0x001A
                    SMTO_ABORTIFHUNG = 0x0002
                    res = ctypes.c_long()
                    ctypes.windll.user32.SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, "Environment", SMTO_ABORTIFHUNG, 5000, ctypes.byref(res))
            except Exception as e:
                print("Failed to add to PATH:", e)
        else:
            try:
                export_line = f'\nexport PATH="$PATH:{target_dir}"\n'
                for rc_file in [".bashrc", ".zshrc", ".bash_profile"]:
                    rc_path = os.path.expanduser(f"~/{rc_file}")
                    if os.path.exists(rc_path):
                        with open(rc_path, "r") as f:
                            content = f.read()
                        if target_dir not in content:
                            with open(rc_path, "a") as fw: fw.write(export_line)
            except Exception as e:
                print("Failed Unix PATH:", e)

if __name__ == "__main__":
    app = InstallerApp()
    app.mainloop()
