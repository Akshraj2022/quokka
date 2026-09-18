    def create_finish_page(self):
        frame = ttk.Frame(self.container)
        ttk.Label(frame, text="Installation Complete!", font=("Segoe UI", 16, "bold")).pack(pady=20)
        if self.img_logo: ttk.Label(frame, image=self.img_logo).pack(pady=10)
        
        self.finish_lbl = ttk.Label(frame, text="Quokka has been installed successfully.")
        self.finish_lbl.pack(pady=10)
        
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
            
            time.sleep(0.5)
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
                
            if self.var_path.get():
                self.update_progress(70, "Updating PATH...")
                self.add_to_path(target_dir)
                
            if self.var_assoc.get():
                self.update_progress(85, "Associating .qko and .🌿 files...")
                self.associate_files(target_dir)
                
            self.update_progress(100, "Finishing up...")
            time.sleep(0.5)
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
                # Broadcast environment change
                import ctypes
                HWND_BROADCAST = 0xFFFF
                WM_SETTINGCHANGE = 0x001A
                ctypes.windll.user32.SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, "Environment", 2, 1000, None)
            winreg.CloseKey(key)
        except Exception as e:
            print("Failed to add to PATH:", e)

    def associate_files(self, target_dir):
        try:
            qk_exe = os.path.join(target_dir, "quokka.bat")
            # Write to HKCU to avoid needing admin privileges if running as user
            for ext in [".qko", ".🌿"]:
                key_ext = winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\{ext}")
                winreg.SetValue(key_ext, "", winreg.REG_SZ, "QuokkaScript")
                winreg.CloseKey(key_ext)
            
            key_cls = winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Classes\QuokkaScript")
            winreg.SetValue(key_cls, "", winreg.REG_SZ, "Quokka Script File")
            
            key_cmd = winreg.CreateKey(key_cls, r"shell\open\command")
            winreg.SetValue(key_cmd, "", winreg.REG_SZ, f'"{qk_exe}" run "%1"')
            winreg.CloseKey(key_cmd)
            winreg.CloseKey(key_cls)
        except Exception as e:
            print("Failed to associate .qko and .🌿 files:", e)

if __name__ == "__main__":
    app = QuokkaInstaller()
    app.mainloop()
