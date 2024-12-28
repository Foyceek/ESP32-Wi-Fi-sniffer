
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, Toplevel, Label, Text, Scrollbar
import threading
import os
from process_files import process_files

class AnalysisToolApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Analysis Tool")

        # Global dictionary to store tabs by their names
        self.tab_references = {}

        # Define dimensions for the window
        w, h = 1000, 700
        ws, hs = self.root.winfo_screenwidth(), self.root.winfo_screenheight()
        x, y = (ws / 2) - (w / 2), (hs / 2) - (h / 2)
        self.root.geometry(f"{w}x{h}+{int(x)}+{int(y)}")

        # Notebook setup
        self.notebook = ttk.Notebook(self.root)
        self.notebook.grid(row=0, column=0, sticky="nsew")

        # Configure grid expansion
        self.root.grid_rowconfigure(0, weight=1)
        self.root.grid_columnconfigure(0, weight=1)

        # Add Instructions Tab before Setup Tab
        self.instructions_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.instructions_tab, text="Instructions")

        # Frame to hold the instructions content
        self.instructions_frame = ttk.Frame(self.instructions_tab)
        self.instructions_frame.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)

        # Add a Text widget to display instructions with a scrollbar
        self.instructions_text = Text(self.instructions_frame, wrap="word", height=15, width=80)
        self.instructions_text.grid(row=0, column=0, sticky="nsew")

        # Configure grid expansion for instructions_frame and the instructions tab
        self.instructions_tab.grid_rowconfigure(0, weight=1)
        self.instructions_tab.grid_columnconfigure(0, weight=1)
        self.instructions_frame.grid_rowconfigure(0, weight=1)
        self.instructions_frame.grid_columnconfigure(0, weight=1)

        # Scrollbar for the Text widget
        self.scrollbar = Scrollbar(self.instructions_frame, command=self.instructions_text.yview)
        self.scrollbar.grid(row=0, column=1, sticky="ns")
        self.instructions_text.config(yscrollcommand=self.scrollbar.set)

        # Add some default instructions text
        instructions = """
Welcome to the Probe Request Analysis Tool!

This tool allows you to process .pcap and .csv files.

To begin, use the Setup tab to select the input directory containing .pcap files 
and REDUCED_DATA.csv file from your capture session.
REDUCED_DATA.csv is only used for Reduced analysis, so it isn't mandatory.

If you already have these files preprocessed, you can select the Quick analysis option.

        """
        self.instructions_text.insert(tk.END, instructions)
        self.instructions_text.config(state=tk.DISABLED)  # Make text non-editable

        # Setup tab
        self.setup_tab = ttk.Frame(self.notebook)
        self.notebook.add(self.setup_tab, text="Setup")

        # Frame to hold the setup form and the table
        self.main_frame = ttk.Frame(self.setup_tab)
        self.main_frame.grid(row=0, column=0, sticky="nsew")
        self.setup_tab.grid_rowconfigure(0, weight=1)
        self.setup_tab.grid_columnconfigure(0, weight=1)

        # Frame for the setup buttons and checkboxes (left side)
        self.setup_frame = ttk.Frame(self.main_frame, padding=10, relief="groove")
        self.setup_frame.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)

        # Configure the layout of setup_frame to prevent resizing
        self.setup_frame.grid_columnconfigure(0, weight=1)

        # Set focus to the "Setup" tab when the app starts
        self.notebook.select(self.setup_tab)

        # Directory selection section
        self.input_directory = ""
        self.output_directory = ""

        self.input_dir_button = ttk.Button(self.setup_frame, text="Select Input Directory", command=self.select_input_directory)
        self.input_dir_button.grid(row=0, column=0, pady=5, sticky="ew")
        self.input_dir_label = ttk.Label(self.setup_frame, text="No directory selected.")
        self.input_dir_label.grid(row=1, column=0, sticky="w")

        self.quick_dir_var = tk.BooleanVar(value=False)
        self.quick_dir_checkbox = ttk.Checkbutton(self.setup_frame, text="Quick analysis", 
                                                  variable=self.quick_dir_var, command=self.quick_directory_selection)
        self.quick_dir_checkbox.grid(row=2, column=0, pady=5, sticky="w")

        self.reduced_var = tk.BooleanVar(value=False)
        self.reduced_checkbox = ttk.Checkbutton(self.setup_frame, text="Reduced analysis", 
                                                 variable=self.reduced_var, command=self.reduced_selection)
        self.reduced_checkbox.grid(row=3, column=0, pady=5, sticky="w")

        self.use_same_directory_var = tk.BooleanVar(value=True)
        self.same_dir_checkbox = ttk.Checkbutton(self.setup_frame, text="Use same directory for input and output", variable=self.use_same_directory_var, command=self.toggle_output_directory_selection)
        self.same_dir_checkbox.grid(row=4, column=0, pady=5, sticky="w")

        self.output_dir_button = ttk.Button(self.setup_frame, text="Select Output Directory (Optional)", command=self.select_output_directory)
        self.output_dir_button.grid(row=5, column=0, pady=5, sticky="ew")
        self.output_dir_label = ttk.Label(self.setup_frame, text="Output Directory: Using input directory.")
        self.output_dir_label.grid(row=6, column=0, sticky="w")

        # Add a variable and checkbox for anonymization
        self.anonymize_var = tk.BooleanVar(value=True)
        self.anonymize_checkbox = ttk.Checkbutton(self.setup_frame, text="Enable Anonymization", variable=self.anonymize_var)
        self.anonymize_checkbox.grid(row=7, column=0, sticky="w")

        self.create_table_var = tk.BooleanVar(value=True)
        self.create_table_checkbox = ttk.Checkbutton(self.setup_frame, text="Create table", variable=self.create_table_var)
        self.create_table_checkbox.grid(row=8, column=0, sticky="w")

        # Create a validation command for the entry widget
        vcmd = (self.setup_frame.register(self.validate_input), '%S', '%P')

        # Add a label
        self.time_entry_label = tk.Label(self.setup_frame, text="Enter time resolution (min):")
        self.time_entry_label.grid(row=9, column=0, sticky="w")

        # Create the entry widget with the validation
        self.time_entry = tk.Entry(self.setup_frame, validate="key", validatecommand=vcmd)
        self.time_entry.grid(row=10, column=0, sticky="ew")

        # Bind focusout event to set default value
        self.time_entry.bind("<FocusOut>", self.set_default_value)
        self.time_entry.insert(0, "15")

        # Analysis options section
        self.options_label = ttk.Label(self.setup_frame, text="Select Analysis Options:")
        self.options_label.grid(row=11, column=0, pady=10)

        self.select_all_var = tk.BooleanVar(value=True)
        self.select_all_checkbox = ttk.Checkbutton(self.setup_frame, text="Select/Deselect All", variable=self.select_all_var, command=self.toggle_all_checkboxes)
        self.select_all_checkbox.grid(row=12, column=0, sticky="w")

        self.checkbox_var1 = tk.BooleanVar(value=True)
        self.checkbox_var2 = tk.BooleanVar(value=True)
        self.checkbox_var3 = tk.BooleanVar(value=True)
        self.checkbox_var4 = tk.BooleanVar(value=True)
        self.checkbox_var5 = tk.BooleanVar(value=True)

        self.option_1 = "Packets Over Time"
        self.option_2 = "RSSI Ranges"
        self.option_3 = "MAC Address Types"
        self.option_4 = "SSID Targets"
        self.option_5 = "CDF"

        self.checkbox1 = ttk.Checkbutton(self.setup_frame, text=self.option_1, variable=self.checkbox_var1, command=self.toggle_time_entry)
        self.checkbox2 = ttk.Checkbutton(self.setup_frame, text=self.option_2, variable=self.checkbox_var2)
        self.checkbox3 = ttk.Checkbutton(self.setup_frame, text=self.option_3, variable=self.checkbox_var3)
        self.checkbox4 = ttk.Checkbutton(self.setup_frame, text=self.option_4, variable=self.checkbox_var4)
        self.checkbox5 = ttk.Checkbutton(self.setup_frame, text=self.option_5, variable=self.checkbox_var5)

        self.checkbox1.grid(row=13, column=0, sticky="w")
        self.checkbox2.grid(row=14, column=0, sticky="w")
        self.checkbox3.grid(row=15, column=0, sticky="w")
        self.checkbox4.grid(row=16, column=0, sticky="w")
        self.checkbox5.grid(row=17, column=0, sticky="w")

        self.checkbox1.config(state="enabled")
        self.checkbox2.config(state="enabled")
        self.checkbox3.config(state="enabled")
        self.checkbox4.config(state="enabled")
        self.checkbox5.config(state="enabled")

        self.start_analysis_button = ttk.Button(self.setup_frame, text="Start Analysis", command=self.start_analysis)
        self.start_analysis_button.grid(row=18, column=0, pady=10, sticky="ew")

        # Progress bar
        self.progress_bar = ttk.Progressbar(self.setup_frame, orient="horizontal", mode="determinate")
        self.progress_bar.grid(row=19, column=0, pady=10, sticky="ew")

        # Label for current file being processed
        self.current_file_label = ttk.Label(self.setup_frame, text="Waiting to start processing.")
        self.current_file_label.grid(row=20, column=0)
        
        # Tooltip information
        tooltips = [
            (self.input_dir_button, "Select directory containing .pcap files from the sniffer"),
            (self.quick_dir_checkbox, "Analyses already preprocessed files"),
            (self.reduced_checkbox, "Analyses REDUCED_DATA"),
            (self.same_dir_checkbox, "Creates 'analyzed_output' directory inside the input directory"),
            (self.output_dir_button, "Select directory to store analysis files"),
            (self.anonymize_checkbox, "Anonymizes the input data"),
            (self.create_table_checkbox, "Creates table overviewing information elements"),
            (self.time_entry_label, "Enter the time resolution for 'Packets Over Time' plot"),
            (self.select_all_checkbox, "Select/Deselect all plot options")
        ]

        # Create tooltips
        for widget, tooltip in tooltips:
            self.create_tooltip(widget, tooltip)

        # Frame for the table (right side)
        self.table_frame = ttk.Frame(self.main_frame, padding=10, relief="groove")
        self.table_frame.grid(row=0, column=1, sticky="nsew", padx=10, pady=10)
        self.main_frame.grid_columnconfigure(1, weight=1)
        self.main_frame.grid_rowconfigure(0, weight=1)

    def toggle_time_entry(self):
        if self.checkbox_var1.get():
            self.time_entry.config(state="normal")
        else:
            self.time_entry.config(state="disabled")

    def validate_input(self, char, current_value):
        """Validate input to allow only positive integers or an empty field."""
        if current_value == "" or (char.isdigit() and current_value.isdigit() and int(current_value) >= 0):
            return True
        return False

    def set_default_value(self, event):
        """Set default value (15) if the entry is left empty."""
        if self.time_entry.get() == "":
            self.time_entry.insert(0, "15")

    def create_tooltip(self, widget, text):
        """Create a tooltip for a widget."""
        def show_tooltip(event):
            tooltip = Toplevel(widget)
            tooltip.overrideredirect(True)  # Remove window decorations
            tooltip.geometry(f"+{event.x_root + 10}+{event.y_root + 10}")  # Position near cursor
            label = Label(tooltip, text=text, bg="lightyellow", relief="solid", borderwidth=1)
            label.pack()
            widget.tooltip = tooltip  # Attach the tooltip to the widget

        def hide_tooltip(event):
            if hasattr(widget, 'tooltip'):
                widget.tooltip.destroy()
                del widget.tooltip

        widget.bind("<Enter>", show_tooltip)
        widget.bind("<Leave>", hide_tooltip)

    def truncate_path(self, path, max_length=30):
        """Truncate a long path to fit in the label."""
        if len(path) > max_length:
            head, tail = os.path.split(path)
            return f"{head[:10]}.../{tail}" if len(head) > 10 else f".../{tail}"
        return path

    def select_input_directory(self):
        self.input_directory = filedialog.askdirectory()
        if self.input_directory:
            truncated_path = self.truncate_path(self.input_directory)
            self.input_dir_label.config(text=f"Input Directory: {truncated_path}")
            self.create_tooltip(self.input_dir_label, self.input_directory)  # Attach tooltip
        else:
            self.input_dir_label.config(text="No directory selected.")

    def select_output_directory(self):
        self.output_directory = filedialog.askdirectory()
        if self.output_directory:
            truncated_path = self.truncate_path(self.output_directory)
            self.output_dir_label.config(text=f"Output Directory: {truncated_path}")
            self.create_tooltip(self.output_dir_label, self.output_directory)  # Attach tooltip
        else:
            self.output_dir_label.config(text="Output Directory: Using input directory.")

    def quick_directory_selection(self):
        if self.quick_dir_var.get():
            self.use_same_directory_var.set(False)
            self.same_dir_checkbox.config(state="disabled")
            # Disable input directory button and update label
            self.input_dir_button.config(state="disabled")
            self.input_dir_label.config(text="Input Directory: Not required (Quick mode enabled).")
            
            # Ensure output directory button is enabled and remove "Optional" from its text
            self.output_dir_button.config(state="normal", text="Select Output Directory")
            self.output_dir_label.config(text="No directory selected.")
        else:
            self.same_dir_checkbox.config(state="normal")
            # Re-enable input directory button and reset label
            self.input_dir_button.config(state="normal")
            self.input_dir_label.config(text="No directory selected.")
            
            # Restore the original text of the output directory button
            self.output_dir_button.config(text="Select Output Directory (Optional)")
            self.toggle_output_directory_selection()

    def reduced_selection(self):
        # Check if `reduced_var` is set to True, then force `quick_dir_var` to True
        if self.reduced_var.get():
            # Disable anonymization, table creation, and checkbox4 when reduced analysis is active
            self.anonymize_checkbox.config(state="disabled")
            self.create_table_checkbox.config(state="disabled")
            self.checkbox4.config(state="disabled")
            self.checkbox5.config(state="disabled")
            self.anonymize_var.set(False)
            self.create_table_var.set(False)
            self.checkbox_var4.set(False)
            self.checkbox_var5.set(False)
        else:
            self.anonymize_checkbox.config(state="enabled")
            self.create_table_checkbox.config(state="enabled")
            self.checkbox4.config(state="enabled")
            self.checkbox5.config(state="enabled")

    def toggle_output_directory_selection(self):
        if self.use_same_directory_var.get():
            self.output_dir_label.config(text="Output Directory: Using input directory.")
        else:
            self.output_dir_label.config(text="No directory selected.")

    def toggle_all_checkboxes(self):
        # Get the state of the "select all" checkbox
        state = self.select_all_var.get()

        # Get the current state of each checkbox
        state1 = self.checkbox1.cget("state")
        state2 = self.checkbox2.cget("state")
        state3 = self.checkbox3.cget("state")
        state4 = self.checkbox4.cget("state")
        state5 = self.checkbox5.cget("state")

        # Check if each checkbox is enabled or normal and toggle if so
        if state1 in ["enabled"]:
            self.checkbox_var1.set(state)
        if state2 in ["enabled"]:
            self.checkbox_var2.set(state)
        if state3 in ["enabled"]:
            self.checkbox_var3.set(state)
        if state4 in ["enabled"]:
            self.checkbox_var4.set(state)
        if state5 in ["enabled"]:
            self.checkbox_var5.set(state)

    def update_progress_bar(self,value):
        self.progress_bar["value"] = value

    def set_progress_max(self,value):
        self.progress_bar["maximum"] = value  # Dynamically update the maximum

    def start_analysis(self):
        # Disable the start button during analysis
        self.start_analysis_button.config(state="disabled")

        analysis_options = {
            self.option_1 : self.checkbox_var1.get(),
            self.option_2 : self.checkbox_var2.get(),
            self.option_3 : self.checkbox_var3.get(),
            self.option_4 : self.checkbox_var4.get(),
            self.option_5 : self.checkbox_var5.get()
        }

        self.selected_options = [key for key, value in analysis_options.items() if value]

        if not self.selected_options:
            messagebox.showerror("Error", "Please select at least one analysis option.")
            self.start_analysis_button.config(state="normal")
            return

        # Determine the output directory based on the 'Use same directory' option
        if self.use_same_directory_var.get():
            # If 'Use same directory' is checked, create output directory within input directory
            self.output_directory = os.path.join(self.input_directory, "analyzed_output")
            os.makedirs(self.output_directory, exist_ok=True)
        else:
            # Ensure output directory is valid if 'Use same directory' is not checked
            if not self.output_directory:
                messagebox.showerror("Error", "Please select an output directory.")
                self.start_analysis_button.config(state="normal")
                return

        # Dynamically create tabs for selected options
        self.create_tabs_for_options(self.selected_options)

        messagebox.showinfo(
            "Analysis Started",
            f"Analysis started with the following parameters:\n\n"
            f"Input Directory: {self.input_directory}\n"
            f"Output Directory: {self.output_directory}\n"
            f"Selected Options: {', '.join(self.selected_options)}\n"
            f"Anonymization: {'Enabled' if self.anonymize_var.get() else 'Disabled'}"
        )

        # Start processing in a separate thread
        threading.Thread(target=self.perform_analysis).start()

    def perform_analysis(self):
        success = process_files(
            self.input_directory,
            self.output_directory,
            self.selected_options,
            self.time_entry.get() + "min",
            self.quick_dir_var.get(),
            self.anonymize_var.get(),
            self.reduced_var.get(),
            self.create_table_var.get(),
            self.update_progress_bar,
            lambda text: self.current_file_label.config(text=text),
            self.update_table,
            self.tab_references.get("RSSI Ranges"),
            self.tab_references.get("Packets Over Time"),
            self.tab_references.get("MAC Address Types"),
            self.tab_references.get("SSID Targets"),
            self.tab_references.get("CDF"),
            self.set_progress_max
        )

        # Enable the start button again after analysis
        self.start_analysis_button.config(state="normal")

    def create_tabs_for_options(self, selected_options):
        """Create or update tabs in the notebook for selected analysis options."""
        existing_tabs = {self.notebook.tab(tab_id, "text") for tab_id in self.notebook.tabs()}

        for option in selected_options:
            if option not in existing_tabs:
                tab = ttk.Frame(self.notebook)
                self.notebook.add(tab, text=option)

                # Add the tab to the references dictionary
                self.tab_references[option] = tab

    def update_table(self, ie_summary, total_probes):
        """Update the table in the Setup tab with the extracted summary."""
        # Remove any existing table or label to avoid duplicates
        for widget in self.table_frame.winfo_children():
            widget.destroy()

        # Add total probes as a label
        total_probes_label = ttk.Label(self.table_frame, text=f"Total Probes: {total_probes}")
        total_probes_label.pack(pady=10)

        # Create the table
        columns = ("Option", "Status", "Percentage")
        treeview = ttk.Treeview(self.table_frame, columns=columns, show="headings", height=1)
        treeview.heading("Option", text="Information Element", anchor="center")
        treeview.heading("Status", text="Included in Probes", anchor="center")
        treeview.heading("Percentage", text="Percentage [%]", anchor="center")

        # Center align all columns and set narrower widths
        treeview.column("Option", width=150, anchor="center")
        treeview.column("Status", width=100, anchor="center")
        treeview.column("Percentage", width=100, anchor="center")

        # Insert the summary data into the table
        for row in ie_summary:
            treeview.insert("", "end", values=row)

        # Pack the treeview
        treeview.pack(padx=10, pady=10, fill="both", expand=True)

if __name__ == "__main__":
    root = tk.Tk()
    app = AnalysisToolApp(root)
    root.mainloop()