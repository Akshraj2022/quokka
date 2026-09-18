import sys
import os
import zipfile
import shutil
import ctypes
import threading
import time
import winreg
import customtkinter as ctk
from tkinter import messagebox, filedialog
from PIL import Image

# Setup Paths
def resource_path(relative_path):
    if hasattr(sys, "_MEIPASS"):
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.abspath("installer_assets"), relative_path)

def is_admin():
    try: return ctypes.windll.shell32.IsUserAnAdmin()
    except: return False

QUOKKA_FACTS = [
    "Fun Fact: Quokkas are known as the world's happiest animals due to their iconic smiles!",
    "Fun Fact: Quokkas have virtually no fear of humans.",
    "Fun Fact: They are small macropods, making them relatives of kangaroos and wallabies.",
    "Fun Fact: Quokkas store fat in their tails to survive scarce seasons.",
]

class InstallerApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")
        
        self.title("Quokka Setup")
        self.geometry("600x450")
        self.resizable(False, False)
        
        self.pages = {}
        self.current_page = None
        
        # Load Assets
        try:
            self.img_logo = ctk.CTkImage(light_image=Image.open(resource_path("cfbl.png")), dark_image=Image.open(resource_path("cfbl.png")), size=(120, 120))
            self.img_txtlogo = ctk.CTkImage(light_image=Image.open(resource_path("txtcfbl.png")), dark_image=Image.open(resource_path("txtcfbl.png")), size=(200, 50))
            self.img_sticker = ctk.CTkImage(light_image=Image.open(resource_path("sticker.jpg")), dark_image=Image.open(resource_path("sticker.jpg")), size=(150, 150))
        except Exception as e:
            print("Error loading images:", e)
            self.img_logo, self.img_txtlogo, self.img_sticker = None, None, None
            
        self.setup_ui()
        
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

    def create_welcome_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        ctk.CTkLabel(frame, text="").pack(pady=5)
        if self.img_logo: ctk.CTkLabel(frame, text="", image=self.img_logo).pack(pady=20)
        
        ctk.CTkLabel(frame, text="Quokka", font=("Segoe UI", 36, "bold"), text_color="#3B8ED0").pack()
        ctk.CTkLabel(frame, text="Version 1.0.0 Setup", font=("Segoe UI", 14), text_color="gray").pack(pady=5)
        
        btn_frame = ctk.CTkFrame(frame, fg_color="transparent")
        btn_frame.pack(side="bottom", fill="x", pady=20, padx=20)
        ctk.CTkButton(btn_frame, text="Next >", command=lambda: self.show_page("options"), width=100).pack(side="right")
        ctk.CTkButton(btn_frame, text="Cancel", command=self.destroy, fg_color="gray", hover_color="#555555", width=100).pack(side="right", padx=10)
        return frame

    def create_options_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        ctk.CTkLabel(frame, text="Installation Options", font=("Segoe UI", 24, "bold")).pack(pady=30)
        
        path_frame = ctk.CTkFrame(frame, fg_color="transparent")
        path_frame.pack(fill="x", padx=40, pady=10)
        ctk.CTkLabel(path_frame, text="Install Location:", font=("Segoe UI", 12)).pack(anchor="w")
        
        default_path = os.path.join(os.environ.get("LOCALAPPDATA", "C:\\\\"), "Programs", "Quokka")
        if is_admin(): default_path = os.path.join(os.environ.get("PROGRAMFILES", "C:\\\\Program Files"), "Quokka")
            
        self.path_var = ctk.StringVar(value=default_path)
        path_row = ctk.CTkFrame(path_frame, fg_color="transparent")
        path_row.pack(fill="x", pady=5)
        ctk.CTkEntry(path_row, textvariable=self.path_var).pack(side="left", fill="x", expand=True, padx=(0, 10))
        ctk.CTkButton(path_row, text="Browse...", command=self.browse_path, width=80).pack(side="right")
        
        comp_frame = ctk.CTkFrame(frame, fg_color="#2b2b2b", corner_radius=10)
        comp_frame.pack(fill="x", padx=40, pady=20, ipady=10, ipadx=10)
        
        self.var_path = ctk.BooleanVar(value=True)
        self.var_assoc = ctk.BooleanVar(value=True)
        self.var_joey = ctk.BooleanVar(value=True)
        
        ctk.CTkCheckBox(comp_frame, text="Add Quokka to PATH", variable=self.var_path).pack(anchor="w", pady=5, padx=10)
        ctk.CTkCheckBox(comp_frame, text="Associate .qk files with Quokka", variable=self.var_assoc).pack(anchor="w", pady=5, padx=10)
        ctk.CTkCheckBox(comp_frame, text="Install Joey ML Extension", variable=self.var_joey).pack(anchor="w", pady=5, padx=10)
        
        btn_frame = ctk.CTkFrame(frame, fg_color="transparent")
        btn_frame.pack(side="bottom", fill="x", pady=20, padx=20)
        ctk.CTkButton(btn_frame, text="Install", command=self.start_install, width=100).pack(side="right")
        ctk.CTkButton(btn_frame, text="< Back", command=lambda: self.show_page("welcome"), fg_color="gray", hover_color="#555555", width=100).pack(side="right", padx=10)
        return frame

    def browse_path(self):
        d = filedialog.askdirectory(initialdir=self.path_var.get())
        if d: self.path_var.set(os.path.normpath(d))

    def create_progress_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        ctk.CTkLabel(frame, text="Installing Quokka...", font=("Segoe UI", 20, "bold")).pack(pady=20)
        if self.img_sticker: ctk.CTkLabel(frame, text="", image=self.img_sticker).pack(pady=10)
        
        self.progress_lbl = ctk.CTkLabel(frame, text="Preparing...", text_color="gray")
        self.progress_lbl.pack(pady=(20, 5))
        self.progress_bar = ctk.CTkProgressBar(frame, width=450, mode="determinate")
        self.progress_bar.set(0)
        self.progress_bar.pack(pady=10)
        
        return frame

    def create_finish_page(self):
        frame = ctk.CTkFrame(self.container, fg_color="transparent")
        ctk.CTkLabel(frame, text="Installation Complete!", font=("Segoe UI", 24, "bold"), text_color="#4CAF50").pack(pady=40)
        
        if self.img_txtlogo: ctk.CTkLabel(frame, text="", image=self.img_txtlogo).pack(pady=10)
        
        ctk.CTkLabel(frame, text="Quokka has been successfully installed on your computer.", font=("Segoe UI", 14)).pack(pady=10)
        
        btn_frame = ctk.CTkFrame(frame, fg_color="transparent")
        btn_frame.pack(side="bottom", fill="x", pady=20, padx=20)
        ctk.CTkButton(btn_frame, text="Finish", command=self.destroy, width=100).pack(side="right")
        
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
                self.associate_ext(target_dir)
                
            self.update_progress("Finishing up...", 1.0)
            time.sleep(0.5)
            self.show_page("finish")
        except Exception as e:
            messagebox.showerror("Error", f"Installation failed: {e}")
            self.destroy()

    def add_to_path(self, target_dir):
        pass # Stubs for safety in demo
        
    def associate_ext(self, target_dir):
        pass # Stubs for safety in demo

if __name__ == "__main__":
    app = InstallerApp()
    app.mainloop()

