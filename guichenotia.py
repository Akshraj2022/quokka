import os
import sys
import subprocess
import threading
import tkinter as tk
from tkinter import filedialog, messagebox

try:
    import customtkinter as ctk
except ImportError:
    print("Please install customtkinter: pip install customtkinter")
    sys.exit(1)

ctk.set_appearance_mode("light")

# Color Palette: Shades of brown and white
BG_COLOR = "#FAF6F0"       # Cream/White
TEXT_COLOR = "#4A3628"     # Dark Brown
EDITOR_BG = "#FFFFFF"      # Pure White
ACCENT_BROWN = "#8C6A53"   # Medium Brown
HOVER_BROWN = "#A8846A"    # Lighter Brown
CONSOLE_BG = "#3E2C20"     # Very Dark Brown
CONSOLE_TEXT = "#F3EBE1"   # Light Cream

class Guichenotia(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Guichenotia - Quokka IDLE")
        self.geometry("900x700")
        self.configure(fg_color=BG_COLOR)
        
        self.current_file = None

        # --- Top Menu ---
        self.menu_frame = ctk.CTkFrame(self, fg_color=BG_COLOR, corner_radius=0)
        self.menu_frame.pack(fill="x", padx=10, pady=5)
        
        self.btn_open = ctk.CTkButton(self.menu_frame, text="Open", command=self.open_file, 
                                      fg_color=ACCENT_BROWN, hover_color=HOVER_BROWN, text_color="#FFFFFF",
                                      width=80)
        self.btn_open.pack(side="left", padx=5)
        
        self.btn_save = ctk.CTkButton(self.menu_frame, text="Save", command=self.save_file,
                                      fg_color=ACCENT_BROWN, hover_color=HOVER_BROWN, text_color="#FFFFFF",
                                      width=80)
        self.btn_save.pack(side="left", padx=5)

        self.btn_run = ctk.CTkButton(self.menu_frame, text="Run Script", command=self.run_script,
                                     fg_color=ACCENT_BROWN, hover_color=HOVER_BROWN, text_color="#FFFFFF",
                                     font=("Segoe UI", 12, "bold"), width=100)
        self.btn_run.pack(side="left", padx=5)

        # --- Paned Window ---
        self.paned_window = tk.PanedWindow(self, orient=tk.VERTICAL, bg=BG_COLOR, bd=0, sashwidth=5)
        self.paned_window.pack(fill="both", expand=True, padx=10, pady=5)

        # --- Editor Area ---
        self.editor_frame = ctk.CTkFrame(self.paned_window, fg_color=EDITOR_BG, border_color=ACCENT_BROWN, border_width=2)
        
        self.editor = tk.Text(self.editor_frame, wrap="none", undo=True, 
                              bg=EDITOR_BG, fg=TEXT_COLOR, insertbackground=TEXT_COLOR,
                              font=("Consolas", 12), bd=0, highlightthickness=0)
        self.editor.pack(fill="both", expand=True, padx=5, pady=5)
        
        self.paned_window.add(self.editor_frame, stretch="always", minsize=200)

        # --- Console Area ---
        self.console_frame = ctk.CTkFrame(self.paned_window, fg_color=CONSOLE_BG, border_color=ACCENT_BROWN, border_width=2)
        self.console_label = ctk.CTkLabel(self.console_frame, text="Output / REPL", text_color=CONSOLE_TEXT, 
                                          font=("Segoe UI", 10, "bold"), anchor="w")
        self.console_label.pack(fill="x", padx=5, pady=2)
        
        self.console = tk.Text(self.console_frame, wrap="word", 
                               bg=CONSOLE_BG, fg=CONSOLE_TEXT, insertbackground=CONSOLE_TEXT,
                               font=("Consolas", 12), bd=0, highlightthickness=0)
        self.console.pack(fill="both", expand=True, padx=5, pady=5)
        
        self.paned_window.add(self.console_frame, minsize=150)

    def open_file(self):
        filepath = filedialog.askopenfilename(defaultextension=".qk", filetypes=[("Quokka Files", "*.qk;*.qka"), ("All Files", "*.*")])
        if not filepath:
            return
        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()
        self.editor.delete("1.0", tk.END)
        self.editor.insert(tk.END, content)
        self.current_file = filepath
        self.title(f"Guichenotia - {os.path.basename(filepath)}")

    def save_file(self):
        if not self.current_file:
            filepath = filedialog.asksaveasfilename(defaultextension=".qk", filetypes=[("Quokka Files", "*.qk;*.qka")])
            if not filepath:
                return
            self.current_file = filepath
        with open(self.current_file, "w", encoding="utf-8") as f:
            f.write(self.editor.get("1.0", tk.END))
        self.title(f"Guichenotia - {os.path.basename(self.current_file)}")
        
    def write_console(self, text):
        self.console.insert(tk.END, text)
        self.console.see(tk.END)

    def run_script(self):
        content = self.editor.get("1.0", tk.END)
        if not content.strip():
            return
        
        # Save temp file
        temp_file = "guichenotia_temp.qk"
        with open(temp_file, "w", encoding="utf-8") as f:
            f.write(content)
            
        self.console.delete("1.0", tk.END)
        self.write_console(f"> Running script...\n")
        
        # Find Quokka executable
        quokka_exe = "quokka.exe"
        if os.path.exists("build/qk_bootstrap.exe"):
            quokka_exe = "build/qk_bootstrap.exe"
            
        def execute():
            try:
                # If we are using qk_bootstrap, we need to pass quokka_interpreter.qka run
                if "qk_bootstrap" in quokka_exe:
                    cmd = [quokka_exe, "src/quokka_interpreter.qka", "run", temp_file]
                else:
                    cmd = [quokka_exe, "run", temp_file]
                
                process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
                out, err = process.communicate()
                self.write_console(out)
                if err:
                    self.write_console(f"\n[Errors]\n{err}")
                self.write_console(f"\n> Process exited with code {process.returncode}\n")
            except Exception as e:
                self.write_console(f"\n[Execution Error]\n{e}\n")

        threading.Thread(target=execute, daemon=True).start()

if __name__ == "__main__":
    app = Guichenotia()
    app.mainloop()
