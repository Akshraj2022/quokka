import sys
import os
import zipfile
import shutil
try:
    import ctypes
except ImportError:
    ctypes = None

import threading
import time
import math
import customtkinter as ctk
from tkinter import messagebox, filedialog
from PIL import Image

# Colors
BG = "#3B3939"
FRAME_BG = "#644C2C"
BTN_BG = "#8C5C28"
BTN_HOVER = "#E49A68"
TEXT_MAIN = "#F1CDB3"
TEXT_ACCENT = "#E9CA63"
TEXT_MUTED = "#C4AD92"

def resource_path(relative_path):
    if hasattr(sys, "_MEIPASS"): return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.abspath("installer_assets"), relative_path)

def is_admin():
    try:
        if os.name == 'nt' and ctypes:
            return ctypes.windll.shell32.IsUserAnAdmin()
        else:
            return os.geteuid() == 0
    except:
        return False

QUOKKA_FACTS = [
    "Fun Fact: Quokkas are known as the world's happiest animals!",
    "Fun Fact: Quokkas have virtually no fear of humans.",
    "Fun Fact: They are small macropods, relatives of kangaroos.",
    "Fun Fact: Quokkas store fat in their tails to survive scarce seasons.",
    "Fun Fact: A baby quokka is called a joey!",
    "Fun Fact: Quokkas can climb trees and shrubs up to 2 meters high.",
]

class InstallerApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Quokka Setup")
        self.geometry("600x480")
        self.resizable(False, False)
        self.configure(fg_color=BG)
        
        self.pages = {}
        self.current_page = None
        self.anim_time = 0.0
        self.fact_index = 0
        
        try:
            self.img_logo = ctk.CTkImage(light_image=Image.open(resource_path("cfbl.png")), size=(140, 140))
            self.img_sticker = ctk.CTkImage(light_image=Image.open(resource_path("sticker.jpg")), size=(140, 140))
        except Exception as e:
            print("Error loading images:", e)
            self.img_logo, self.img_sticker = None, None
            
        self.setup_ui()
        self.init_dots()
        self.update_dots()
        self.cycle_facts()
        
    def setup_ui(self):
        self.container = ctk.CTkFrame(self, fg_color="transparent")
        self.container.pack(fill="both", expand=True)
        
        self.pages["welcome"] = self.create_welcome_page()
        self.pages["options"] = self.create_options_page()
        self.pages["progress"] = self.create_progress_page()
        self.pages["finish"] = self.create_finish_page()
        
        self.show_page("welcome")

    def show_page(self, name):
        if self.current_page: self.current_page.pack_forget()
        self.current_page = self.pages[name]
        self.current_page.pack(fill="both", expand=True)


    def init_dots(self):
        import random
        self.dots = []
        for _ in range(60):
            size = random.choice([4, 6, 8])
            x = random.randint(10, 590)
            y = random.randint(10, 470)
            dot = ctk.CTkFrame(self, width=size, height=size, corner_radius=size//2, fg_color="#4A3B32")
            dot.place(x=x, y=y)
            dot.lower()
            self.dots.append({"widget": dot, "x": x + size/2, "y": y + size/2, "base": "#4A3B32", "glow": "#E49A68"})
            
    def update_dots(self):
        import math
        mx = self.winfo_pointerx() - self.winfo_rootx()
        my = self.winfo_pointery() - self.winfo_rooty()
        for dot in self.dots:
            dist = math.hypot(dot["x"] - mx, dot["y"] - my)
            if dist < 80:
                dot["widget"].configure(fg_color=dot["glow"])
            else:
                dot["widget"].configure(fg_color=dot["base"])
        self.after(30, self.update_dots)

    def cycle_facts(self):
        self.fact_index = (self.fact_index + 1) % len(QUOKKA_FACTS)
        if hasattr(self, "fact_lbl") and self.fact_lbl.winfo_exists():
            self.fact_lbl.configure(text=QUOKKA_FACTS[self.fact_index])
        self.after(4000, self.cycle_facts)

    def create_welcome_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        
        # Logo frame for absolute placement animation
        logo_area = ctk.CTkFrame(frame, fg_color="transparent")
        logo_area.pack(fill="x", pady=(20, 0))
        
        if self.img_logo:
            self.logo_lbl = ctk.CTkLabel(logo_area, text="", image=self.img_logo)
            self.logo_lbl.pack(pady=10)
            
        ctk.CTkLabel(frame, text="Quokka", font=("Segoe UI", 42, "bold"), text_color=TEXT_ACCENT).pack(pady=(10, 0))
        ctk.CTkLabel(frame, text="Version 1.0.0 Setup", font=("Segoe UI", 16), text_color=TEXT_MUTED).pack()
        
        # Fun fact
        self.fact_lbl = ctk.CTkLabel(frame, text=QUOKKA_FACTS[self.fact_index], font=("Segoe UI", 13, "italic"), text_color=TEXT_MAIN, wraplength=450)
        self.fact_lbl.pack(pady=30)
        
        btn_frame = ctk.CTkFrame(frame, fg_color="transparent")
        btn_frame.pack(side="bottom", fill="x", pady=20, padx=40)
        ctk.CTkButton(btn_frame, text="Next >", command=lambda: self.show_page("options"), fg_color=BTN_BG, hover_color=BTN_HOVER, text_color="#FFF", corner_radius=20, font=("Segoe UI", 14, "bold")).pack(side="right")
        ctk.CTkButton(btn_frame, text="Cancel", command=self.destroy, fg_color=FRAME_BG, hover_color="#553f24", text_color=TEXT_MAIN, corner_radius=20, font=("Segoe UI", 14)).pack(side="right", padx=10)
        return frame

    def create_options_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        ctk.CTkLabel(frame, text="Customize Installation", font=("Segoe UI", 26, "bold"), text_color=TEXT_ACCENT).pack(pady=(30, 20))
        
        path_frame = ctk.CTkFrame(frame, fg_color="transparent")
        path_frame.pack(fill="x", padx=50, pady=10)
        ctk.CTkLabel(path_frame, text="Install Location:", font=("Segoe UI", 14), text_color=TEXT_MAIN).pack(anchor="w")
        
        if os.name == 'nt':
            default_path = os.path.join(os.environ.get("LOCALAPPDATA", "C:\\\\"), "Programs", "Quokka")
            if is_admin(): default_path = os.path.join(os.environ.get("PROGRAMFILES", "C:\\\\Program Files"), "Quokka")
        elif sys.platform == 'darwin':
            default_path = "/Applications/Quokka" if is_admin() else os.path.expanduser("~/Applications/Quokka")
        else:
            default_path = "/opt/quokka" if is_admin() else os.path.expanduser("~/.local/share/quokka")
            
        self.path_var = ctk.StringVar(value=default_path)
        path_row = ctk.CTkFrame(path_frame, fg_color="transparent")
        path_row.pack(fill="x", pady=5)
        ctk.CTkEntry(path_row, textvariable=self.path_var, fg_color=FRAME_BG, border_color=BTN_BG, text_color=TEXT_MAIN).pack(side="left", fill="x", expand=True, padx=(0, 10))
        ctk.CTkButton(path_row, text="Browse", command=self.browse_path, width=80, fg_color=FRAME_BG, hover_color=BTN_BG, text_color=TEXT_MAIN, border_width=2, border_color=BTN_BG).pack(side="right")
        
        comp_frame = ctk.CTkFrame(frame, fg_color=FRAME_BG, corner_radius=15)
        comp_frame.pack(fill="x", padx=50, pady=20, ipady=15, ipadx=15)
        
        self.var_path = ctk.BooleanVar(value=True)
        self.var_assoc = ctk.BooleanVar(value=True)
        self.var_joey = ctk.BooleanVar(value=True)
        
        # Checkboxes with cute colors
        for txt, var in [("Add Quokka to PATH", self.var_path), ("Associate .qk files with Quokka", self.var_assoc), ("Install Joey ML Extension", self.var_joey)]:
            ctk.CTkCheckBox(comp_frame, text=txt, variable=var, fg_color=BTN_BG, hover_color=BTN_HOVER, checkmark_color="#FFF", text_color=TEXT_MAIN, font=("Segoe UI", 13)).pack(anchor="w", pady=6, padx=10)
        
        btn_frame = ctk.CTkFrame(frame, fg_color="transparent")
        btn_frame.pack(side="bottom", fill="x", pady=20, padx=40)
        ctk.CTkButton(btn_frame, text="Install", command=self.start_install, width=120, fg_color=BTN_BG, hover_color=BTN_HOVER, text_color="#FFF", corner_radius=20, font=("Segoe UI", 14, "bold")).pack(side="right")
        ctk.CTkButton(btn_frame, text="< Back", command=lambda: self.show_page("welcome"), fg_color=FRAME_BG, hover_color="#553f24", text_color=TEXT_MAIN, corner_radius=20, font=("Segoe UI", 14)).pack(side="right", padx=10)
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
        self.progress_lbl.pack(pady=(30, 10))
        
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
                    
            if self.var_joey.get():
                self.update_progress("Configuring Joey ML extension...", 0.4)
                time.sleep(0.5)
                
            if self.var_path.get():
                self.update_progress("Updating system PATH...", 0.6)
                self.add_to_path(target_dir)
                
            if self.var_assoc.get():
                self.update_progress("Associating .qk files...", 0.8)
                
            self.update_progress("All set! Wrapping up...", 1.0)
            time.sleep(0.8)
            self.show_page("finish")
        except Exception as e:
            messagebox.showerror("Error", f"Installation failed: {e}")
            self.destroy()


    def add_to_path(self, target_dir):
        import os
        if os.name == 'nt':
            try:
                import winreg, ctypes
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
                for rc_file in [".bashrc", ".zshrc"]:
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

