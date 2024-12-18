import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import threading
from process_files import process_files

def select_input_directory():
    global input_directory
    input_directory = filedialog.askdirectory()
    if input_directory:
        input_dir_label.config(text=f"Input Directory: {input_directory}")
    else:
        input_dir_label.config(text="No directory selected.")

def select_output_directory():
    global output_directory
    output_directory = filedialog.askdirectory()
    if output_directory:
        output_dir_label.config(text=f"Output Directory: {output_directory}")
    else:
        output_dir_label.config(text="No directory selected (using input directory).")

def quick_directory_selection():
    if quick_dir_var.get():
        use_same_directory_var.set(False)
        # Disable input directory button and update label
        input_dir_button.config(state="disabled")
        input_dir_label.config(text="Input Directory: Not required (Quick mode enabled).")
        
        # Ensure output directory button is enabled and remove "Optional" from its text
        output_dir_button.config(state="normal", text="Select Output Directory")
        output_dir_label.config(text="No directory selected.")
    else:
        # Re-enable input directory button and reset label
        input_dir_button.config(state="normal")
        input_dir_label.config(text="No directory selected.")
        
        # Restore the original text of the output directory button
        output_dir_button.config(text="Select Output Directory (Optional)")
        toggle_output_directory_selection()

def toggle_output_directory_selection():
    if use_same_directory_var.get():
        output_dir_button.config(state="disabled")
        output_dir_label.config(text="Output Directory: Using input directory.")
    else:
        output_dir_button.config(state="normal")
        output_dir_label.config(text="No directory selected.")


def toggle_all_checkboxes():
    state = select_all_var.get()
    checkbox_var1.set(state)
    checkbox_var2.set(state)
    checkbox_var3.set(state)
    checkbox_var4.set(state)

def update_progress_bar(value):
    progress_bar["value"] = value

def set_progress_max(value):
    progress_bar["maximum"] = value  # Dynamically update the maximum

def start_analysis():
    # Skip input directory validation if quick mode is enabled
    if not quick_dir_var.get() and not input_directory:
        messagebox.showerror("Error", "Please select an input directory.")
        return

    analysis_options = {
        option_1: checkbox_var1.get(),
        option_2: checkbox_var2.get(),
        option_3: checkbox_var3.get(),
        option_4: checkbox_var4.get()
    }

    selected_options = [key for key, value in analysis_options.items() if value]

    if not selected_options:
        messagebox.showerror("Error", "Please select at least one analysis option.")
        return

    output_dir = input_directory if use_same_directory_var.get() else output_directory
    if not output_dir:
        messagebox.showerror("Error", "Please select an output directory or enable 'Use same directory'.")
        return

    # Dynamically create tabs for selected options
    create_tabs_for_options(selected_options)

    messagebox.showinfo(
        "Analysis Started",
        f"Analysis started with the following parameters:\n\n"
        f"Input Directory: {input_directory}\n"
        f"Output Directory: {output_dir}\n"
        f"Selected Options: {', '.join(selected_options)}\n"
        f"Anonymization: {'Enabled' if anonymize_var.get() else 'Disabled'}"
    )

    # Retrieve tabs from the global dictionary
    rssi_tab = tab_references.get("RSSI Ranges")
    packet_count_tab = tab_references.get("Packets Over Time")
    mac_address_tab = tab_references.get("MAC Address Types")
    ssid_tab = tab_references.get("SSID Targets")

    # Start processing in a separate thread
    thread = threading.Thread(
        target=process_files,
        args=(
            input_directory,
            output_dir,
            selected_options,
            quick_dir_var.get(),
            anonymize_var.get(),
            update_progress_bar,
            lambda text: current_file_label.config(text=text),
            update_table,
            rssi_tab,  
            packet_count_tab,
            mac_address_tab,
            ssid_tab,
            set_progress_max
        )
    )
    thread.start()

def create_tabs_for_options(selected_options):
    """Create or update tabs in the notebook for selected analysis options."""
    global tab_references
    existing_tabs = {notebook.tab(tab_id, "text") for tab_id in notebook.tabs()}

    for option in selected_options:
        if option not in existing_tabs:
            tab = ttk.Frame(notebook)
            notebook.add(tab, text=option)

            # Add the tab to the global dictionary
            tab_references[option] = tab

def update_table(ie_summary, total_probes):
    """Update the table in the Setup tab with the extracted summary."""
    # Remove any existing table or label to avoid duplicates
    for widget in table_frame.winfo_children():
        widget.destroy()

    # Add total probes as a label
    total_probes_label = ttk.Label(table_frame, text=f"Total Probes: {total_probes}")
    total_probes_label.pack(pady=10)

    # Create the table
    columns = ("Option", "Status", "Percentage")
    treeview = ttk.Treeview(table_frame, columns=columns, show="headings", height=1)
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

# Main application window
app = tk.Tk()
app.title("Analysis Tool")

# Global dictionary to store tabs by their names
tab_references = {}

w = 1000  # Increased width for the root window to accommodate the table on the right
h = 700  # height for the Tk root

# get screen width and height
ws = app.winfo_screenwidth()  # width of the screen
hs = app.winfo_screenheight()  # height of the screen

# calculate x and y coordinates for the Tk root window
x = (ws / 2) - (w / 2)
y = (hs / 2) - (h / 2)

# set the dimensions of the screen and where it is placed
app.geometry('%dx%d+%d+%d' % (w, h, x, y))

# Notebook setup
notebook = ttk.Notebook(app)
notebook.pack(fill="both", expand=True)

# Setup tab
setup_tab = ttk.Frame(notebook)
notebook.add(setup_tab, text="Setup")

# Frame to hold the setup form and the table
main_frame = ttk.Frame(setup_tab)
main_frame.pack(fill="both", expand=True)

# Frame for the setup buttons and checkboxes (left side)
setup_frame = ttk.Frame(main_frame, padding=10, relief="groove")
setup_frame.pack(side="left", anchor="nw", padx=10, pady=10, fill="y")

# Directory selection section
input_directory = ""
output_directory = ""

input_dir_button = ttk.Button(setup_frame, text="Select Input Directory", command=select_input_directory)
input_dir_button.pack(pady=5, fill="x")
input_dir_label = ttk.Label(setup_frame, text="No directory selected.")
input_dir_label.pack()

quick_dir_var = tk.BooleanVar(value=False)
quick_dir_checkbox = ttk.Checkbutton(setup_frame, text="Already have files necessary for analysis generated?", 
                                     variable=quick_dir_var, command=quick_directory_selection)
quick_dir_checkbox.pack(pady=5, anchor="w")

use_same_directory_var = tk.BooleanVar(value=True)
same_dir_checkbox = ttk.Checkbutton(setup_frame, text="Use same directory for input and output", variable=use_same_directory_var, command=toggle_output_directory_selection)
same_dir_checkbox.pack(pady=5, anchor="w")

output_dir_button = ttk.Button(setup_frame, text="Select Output Directory (Optional)", command=select_output_directory, state="disabled")
output_dir_button.pack(pady=5, fill="x")
output_dir_label = ttk.Label(setup_frame, text="Output Directory: Using input directory.")
output_dir_label.pack()

# Add a variable and checkbox for anonymization
anonymize_var = tk.BooleanVar(value=True)
anonymize_checkbox = ttk.Checkbutton(setup_frame, text="Enable Anonymization", variable=anonymize_var)
anonymize_checkbox.pack(anchor="w")

# Analysis options section
options_label = ttk.Label(setup_frame, text="Select Analysis Options:")
options_label.pack(pady=10)

option_1 = "Packets Over Time"
option_2 = "RSSI Ranges"
option_3 = "MAC Address Types"
option_4 = "SSID Targets"

select_all_var = tk.BooleanVar(value=True)
select_all_checkbox = ttk.Checkbutton(setup_frame, text="Select/Deselect All", variable=select_all_var, command=toggle_all_checkboxes)
select_all_checkbox.pack(anchor="w")

checkbox_var1 = tk.BooleanVar(value=True)
checkbox_var2 = tk.BooleanVar(value=True)
checkbox_var3 = tk.BooleanVar(value=True)
checkbox_var4 = tk.BooleanVar(value=True)

checkbox1 = ttk.Checkbutton(setup_frame, text=option_1, variable=checkbox_var1)
checkbox2 = ttk.Checkbutton(setup_frame, text=option_2, variable=checkbox_var2)
checkbox3 = ttk.Checkbutton(setup_frame, text=option_3, variable=checkbox_var3)
checkbox4 = ttk.Checkbutton(setup_frame, text=option_4, variable=checkbox_var4)

checkbox1.pack(anchor="w")
checkbox2.pack(anchor="w")
checkbox3.pack(anchor="w")
checkbox4.pack(anchor="w")

start_analysis_button = ttk.Button(setup_frame, text="Start Analysis", command=start_analysis)
start_analysis_button.pack(pady=10, fill="x")

# Progress bar
progress_bar = ttk.Progressbar(setup_frame, orient="horizontal", length=400, mode="determinate")
progress_bar.pack(pady=10)

# Label for current file being processed
current_file_label = ttk.Label(setup_frame, text="Waiting to start processing.")
current_file_label.pack()

# Frame for the table (right side)
table_frame = ttk.Frame(main_frame, padding=10, relief="groove")
table_frame.pack(side="right", fill="both", expand=True, padx=10, pady=10)

# Run the application
app.mainloop()
