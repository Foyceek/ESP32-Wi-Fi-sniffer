import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.cm import viridis
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

def plot_rssi_distribution(csv_file, plot_tab):
    """
    Reads a CSV file and plots a bar graph showing the distribution of RSSI values.

    Args:
        csv_file (str): Path to the CSV file.
        plot_tab (tk.Frame): The Tkinter tab where the plot will be displayed.
    """
    def plot():
        # Check if there is an existing plot and remove it
        for widget in plot_tab.winfo_children():
            widget.destroy()

        # Load the CSV file into a DataFrame
        df = pd.read_csv(csv_file)

        # Count occurrences of each RSSI value
        rssi_counts = df['RSSI'].value_counts().sort_index()

        # Normalize counts for color mapping
        norm = plt.Normalize(vmin=rssi_counts.index.min(), vmax=rssi_counts.index.max())
        colors = viridis(norm(rssi_counts.index))

        # Create the bar plot
        fig, ax = plt.subplots(figsize=(10, 6))
        bars = ax.bar(rssi_counts.index, rssi_counts.values, color=colors, width=1.0, edgecolor='black')

        # Customize the plot
        ax.set_title('RSSI Value Distribution', fontsize=14)
        ax.set_xlabel('RSSI', fontsize=12)
        ax.set_ylabel('Count', fontsize=12)
        ax.grid(axis='y', linestyle='--', alpha=0.7)

        # Add colorbar
        sm = plt.cm.ScalarMappable(cmap='viridis', norm=norm)
        sm.set_array([])
        cbar = fig.colorbar(sm, ax=ax)
        cbar.set_label('RSSI Value', fontsize=12)

        # Embed the plot into the Tkinter tab
        canvas = FigureCanvasTkAgg(fig, master=plot_tab)
        canvas.draw()
        canvas.get_tk_widget().pack(fill="both", expand=True)

    # Schedule the plot function on the main thread
    plot_tab.after(0, plot)
