import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

def plot_packet_count(csv_file, plot_tab, time_resolution='15T'):
    def plot():
        # Check if there is an existing plot and remove it
        for widget in plot_tab.winfo_children():
            widget.destroy()

        # Read the CSV file into a DataFrame
        df = pd.read_csv(csv_file)

        # Combine Date and Time columns into a single datetime column
        df['Datetime'] = pd.to_datetime(df['Date'] + ' ' + df['Time'])

        # Set Datetime as the index for easy resampling
        df.set_index('Datetime', inplace=True)

        # Resample the data by the specified time resolution
        resampled_df = df.resample(time_resolution).size()

        # Plot the packet count over time
        plt.figure(figsize=(10, 6))
        resampled_df.plot()
        plt.title('Packet Count Over Time')
        plt.xlabel('Time')
        plt.ylabel('Packet Count')
        plt.grid(True)

        canvas = FigureCanvasTkAgg(plt.gcf(), master=plot_tab)
        canvas.draw()
        canvas.get_tk_widget().pack(fill="both", expand=True)
        
    plot_tab.after(0, plot)
    
# Example usage: change the file path and time resolution as needed
# plot_packet_count(r"C:\Franta\VUT\5_semestr\SEP\analize_output\anonymized_relevant_data.csv", 
#                   time_resolution='10min')  # Default 15 minutes resolution
