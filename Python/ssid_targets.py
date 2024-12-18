import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

def plot_ssid_groups(csv_file,plot_tab):
    def plot():
        # Check if there is an existing plot and remove it
        for widget in plot_tab.winfo_children():
            widget.destroy()

        # Load the CSV file
        df = pd.read_csv(csv_file)

        # Create a new column to categorize the SSID into 'Wildcard' or 'Targeted'
        df['SSID Group'] = df['Preferred SSIDs'].apply(lambda x: 'Wildcard' if x == 'Wildcard' else 'Targeted')

        # Count the number of packets in each group
        ssid_count = df['SSID Group'].value_counts()

        # Plot the results
        plt.figure(figsize=(6, 4))
        ax = ssid_count.plot(kind='bar', color=['blue', 'red'])
        plt.title('Packets Grouped by SSID Type (Wildcard vs. Targeted)')
        plt.ylabel('Packet Count')
        plt.xticks([0, 1], ['Wildcard (Non-targeted)', 'Targeted'], rotation=0)

        # Add text labels on top of the bars
        for i, v in enumerate(ssid_count):
            ax.text(i, v + 10, str(v), ha='center', va='bottom', fontsize=10)

        canvas = FigureCanvasTkAgg(plt.gcf(), master=plot_tab)
        canvas.draw()
        canvas.get_tk_widget().pack(fill="both", expand=True)
        
    plot_tab.after(0, plot)

# Example usage:
# plot_ssid_groups(r"C:\Franta\VUT\5_semestr\SEP\analize_output\anonymized_relevant_data.csv")  # Replace with your actual file path
