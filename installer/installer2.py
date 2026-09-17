        frame = ttk.Frame(self.container)
        if self.img_logo: ttk.Label(frame, image=self.img_logo).pack(pady=20)
        if self.img_txtlogo: ttk.Label(frame, image=self.img_txtlogo).pack()
        
        ttk.Label(frame, text="Version 1.0.0", font=("Segoe UI", 10, "italic")).pack(pady=5)
        ttk.Label(frame, text="A deterministic, self-hosted language for modern AI workflows.", font=("Segoe UI", 12)).pack(pady=20)
        
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
        self.progress_lbl.pack(pady=10)
        self.progress_bar = ttk.Progressbar(frame, orient=HORIZONTAL, length=400, mode='determinate')
        self.progress_bar.pack(pady=10)
        
        return frame
