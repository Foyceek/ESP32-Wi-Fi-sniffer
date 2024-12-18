import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

def plot_mac_address_types(csv_file,plot_tab):
    # Function to determine if MAC address is randomized
    def is_randomized(mac_address):
        # Extract the first byte of the MAC address
        first_byte = int(mac_address.split(':')[0], 16)
        # Check if the second least significant bit (B1) is set
        return (first_byte & 0x02) != 0
    
    def plot():
        # Check if there is an existing plot and remove it
        for widget in plot_tab.winfo_children():
            widget.destroy()

        # Load the CSV file
        df = pd.read_csv(csv_file)

        # Apply the function to check for randomized MAC addresses
        df['Is Randomized'] = df['Device MAC'].apply(is_randomized)

        # Count the number of unique and randomized MAC addresses
        unique_count = df['Is Randomized'].value_counts()

        # Plot the results
        plt.figure(figsize=(6, 4))
        ax = unique_count.plot(kind='bar', color=['blue', 'red'])
        plt.title('Randomized vs. Globally Unique MAC Addresses')
        plt.ylabel('Count')
        plt.xticks([0, 1], ['Globally Unique', 'Randomized'], rotation=0)

        # Add text labels on top of the bars
        for i, v in enumerate(unique_count):
            ax.text(i, v + 10, str(v), ha='center', va='bottom', fontsize=10)

        canvas = FigureCanvasTkAgg(plt.gcf(), master=plot_tab)
        canvas.draw()
        canvas.get_tk_widget().pack(fill="both", expand=True)

    plot_tab.after(0, plot)

# Example usage:
# plot_mac_address_types(r"C:\Franta\VUT\5_semestr\SEP\analize_output\anonymized_relevant_data.csv")  # Replace with your actual file path