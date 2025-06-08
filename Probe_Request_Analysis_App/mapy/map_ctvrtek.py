import pandas as pd
import matplotlib.pyplot as plt
import sys
from matplotlib.ticker import MaxNLocator

# Set encoding for proper Czech character handling
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

# Read the CSV data with proper encoding
df = pd.read_csv('ctvrtek_final.csv', encoding='utf-8')

# Read packet data
packet_df = pd.read_csv('anonymized_relevant_data_ctvrtek.csv')
packet_df['Time'] = pd.to_datetime(packet_df['Time'], format='%H:%M:%S.%f')
packet_df['time_minutes'] = packet_df['Time'].dt.hour * 60 + packet_df['Time'].dt.minute + packet_df['Time'].dt.second / 60

# Filter out BREAK rows and convert arrival time to datetime
non_break_mask = df['Stop Name'] != 'BREAK'
df.loc[non_break_mask, 'arrival_datetime'] = pd.to_datetime(df.loc[non_break_mask, 'Arrival Time'], format='%H:%M:%S')
df.loc[non_break_mask, 'arrival_minutes'] = df.loc[non_break_mask, 'arrival_datetime'].dt.hour * 60 + df.loc[non_break_mask, 'arrival_datetime'].dt.minute

# MANUAL STOP ORDER DEFINITION
STOP_ORDER = [
    "Vozovna Pisárky",
    "ARENA BRNO",
    "Výstaviště - vstup G2",
    "Výstaviště - hlavní vstup",
    "Mendlovo náměstí",
    "Václavská",
    "Hybešova",
    "Nové sady",
    "Čertova rokle - nástup",
    "Čertova rokle - výstup",
    "Halasovo náměstí",
    "Fügnerova",
    "Bieblova",
    "Lesnická",
    "Zemědělská",
    "Tomanova",
    "Jugoslávská",
    "Dětská nemocnice",
    "Náměstí 28. října",
    "Moravské náměstí",
    "Česká",
    "Náměstí Svobody",
    "Zelný trh",
    "Hlavní nádraží",
    "Vlhká",
    "Masná",
    "Životského",
    "Geislerova",
    "Buzkova",
    "Otakara Ševčíka",
    "Dělnický dům",
    "Juliánov - nástup",
    "Juliánov - výstup",

]

def detect_break_segments(df):
    """
    Detect segments based on BREAK,BREAK entries in CSV
    """
    segments = []
    current_segment_data = []
    segment_id = 1
    
    for i, row in df.iterrows():
        if row['Stop Name'] == 'BREAK' and row['Arrival Time'] == 'BREAK':
            # End current segment if it has data
            if current_segment_data:
                segments.append({
                    'segment_id': segment_id,
                    'start_stop': current_segment_data[0]['stop'],
                    'end_stop': current_segment_data[-1]['stop'],
                    'description': f"Segment {segment_id}",
                    'data': current_segment_data,
                    'start_time': current_segment_data[0]['time'],
                    'end_time': current_segment_data[-1]['time'],
                    'duration': current_segment_data[-1]['time'] - current_segment_data[0]['time']
                })
                segment_id += 1
                current_segment_data = []
        else:
            # Add stop to current segment
            current_segment_data.append({
                'stop': row['Stop Name'],
                'time': row['arrival_minutes'],
                'time_str': row['Arrival Time'][:5]
            })
    
    # Add final segment if exists
    if current_segment_data:
        segments.append({
            'segment_id': segment_id,
            'start_stop': current_segment_data[0]['stop'],
            'end_stop': current_segment_data[-1]['stop'],
            'description': f"Segment {segment_id}",
            'data': current_segment_data,
            'start_time': current_segment_data[0]['time'],
            'end_time': current_segment_data[-1]['time'],
            'duration': current_segment_data[-1]['time'] - current_segment_data[0]['time']
        })
    
    return segments

def calculate_packet_counts_for_segment(segment_data, packet_df, time_window=2):
    """
    Calculate packet counts for each stop in a segment based on time windows
    """
    packet_counts = []
    
    for stop_info in segment_data:
        stop_time = stop_info['time']
        time_start = stop_time - time_window
        time_end = stop_time + time_window
        
        packets_in_window = packet_df[
            (packet_df['time_minutes'] >= time_start) & 
            (packet_df['time_minutes'] <= time_end)
        ]
        
        packet_count = len(packets_in_window)
        packet_counts.append(packet_count)
    
    return packet_counts

def calculate_diagonal_packet_count(end_time, start_time, packet_df, time_window=2):
    """
    Calculate packet count for diagonal connection between segments
    """
    # Use the midpoint time between end of one segment and start of next
    midpoint_time = (end_time + start_time) / 2
    time_start = midpoint_time - time_window
    time_end = midpoint_time + time_window
    
    packets_in_window = packet_df[
        (packet_df['time_minutes'] >= time_start) & 
        (packet_df['time_minutes'] <= time_end)
    ]
    
    return len(packets_in_window)

def create_route_based_stop_layout(segments):
    """
    Create a stop layout based on the manually defined STOP_ORDER
    """
    stops_in_segments = set()
    for segment in segments:
        for stop_data in segment['data']:
            stops_in_segments.add(stop_data['stop'])
    
    all_stops_ordered = []
    stop_to_position = {}
    
    # Add stops from STOP_ORDER that exist in our segments
    for stop in STOP_ORDER:
        if stop in stops_in_segments:
            stop_to_position[stop] = len(all_stops_ordered)
            all_stops_ordered.append(stop)
    
    # Add any remaining stops
    for stop in stops_in_segments:
        if stop not in stop_to_position:
            stop_to_position[stop] = len(all_stops_ordered)
            all_stops_ordered.append(stop)
    
    return all_stops_ordered, stop_to_position

def plot_segments_unified_heatmap(segments, packet_df):
    """
    Create a single figure with all segments using improved layering and toggleable static labels
    """
    if not segments:
        print("No segments found to plot!")
        return []
    
    # Calculate packet counts for all segments
    for segment in segments:
        packet_counts = calculate_packet_counts_for_segment(segment['data'], packet_df)
        segment['packet_counts'] = packet_counts
    
    # Calculate diagonal connection packet counts
    diagonal_connections = []
    for seg_idx in range(len(segments) - 1):
        current_segment = segments[seg_idx]
        next_segment = segments[seg_idx + 1]
        
        end_time = current_segment['data'][-1]['time']
        start_time = next_segment['data'][0]['time']
        
        diagonal_packet_count = calculate_diagonal_packet_count(end_time, start_time, packet_df)
        diagonal_connections.append(diagonal_packet_count)
    
    # Find global min/max for consistent color scaling (including diagonal connections)
    all_counts = []
    for segment in segments:
        all_counts.extend(segment['packet_counts'])
    all_counts.extend(diagonal_connections)
    
    if not all_counts:
        print("No packet data found!")
        return []
    
    global_min = min(all_counts)
    global_max = max(all_counts)
    
    # Create route-based stop layout
    unique_stops, stop_positions = create_route_based_stop_layout(segments)
    
    # Create single large figure with more space at bottom for button
    fig = plt.figure(figsize=(32, 18))
    mng = fig.canvas.manager
    try:
        mng.window.wm_state('zoomed')
    except:
        try:
            mng.full_screen_toggle()
        except:
            try:
                mng.window.showMaximized()
            except:
                pass
    
    # Adjust subplot to leave more room at bottom for button
    ax = fig.add_subplot(1, 1, 1)
    cmap = plt.cm.viridis
    
    # Calculate segment positions
    num_segments = len(segments)
    segment_spacing = 5.0
    total_height = (num_segments - 1) * segment_spacing
    y_start = -total_height / 2
    y_positions = [y_start + i * segment_spacing for i in range(num_segments)]
    
    # Store packet labels for toggle functionality
    packet_labels = []
    
    # LAYER 1: Plot connections first (background layer)
    for seg_idx, segment in enumerate(segments):
        y_pos = y_positions[seg_idx]
        
        segment_x_positions = []
        for stop_data in segment['data']:
            x_pos = stop_positions[stop_data['stop']]
            segment_x_positions.append(x_pos)
        
        packet_counts = segment['packet_counts']
        
        # Normalize packet counts for color mapping
        if global_max > global_min:
            normalized_counts = [(count - global_min) / (global_max - global_min) for count in packet_counts]
        else:
            normalized_counts = [0.5] * len(packet_counts)
        
        # Connect points with lines
        for j in range(len(segment_x_positions) - 1):
            avg_norm_count = (normalized_counts[j] + normalized_counts[j + 1]) / 2
            avg_packet_count = (packet_counts[j] + packet_counts[j + 1]) / 2
            color = cmap(avg_norm_count)
            line = ax.plot([segment_x_positions[j], segment_x_positions[j + 1]], 
                   [y_pos, y_pos], '-', color=color, linewidth=4, alpha=0.7, zorder=1)[0]
            
            # Add static label at midpoint of connection
            mid_x = (segment_x_positions[j] + segment_x_positions[j + 1]) / 2
            label = ax.text(mid_x, y_pos - 0.3, f'{avg_packet_count:.0f}', 
                           ha='center', va='center', fontsize=8, 
                           color='black', zorder=2, visible=True)
            packet_labels.append(label)
    
    # Add diagonal connections between segment endpoints (same style as regular connections)
    for seg_idx in range(len(segments) - 1):
        current_segment = segments[seg_idx]
        next_segment = segments[seg_idx + 1]
        
        # Get end point of current segment
        end_stop = current_segment['data'][-1]['stop']
        end_x = stop_positions[end_stop]
        end_y = y_positions[seg_idx]
        
        # Get start point of next segment
        start_stop = next_segment['data'][0]['stop']
        start_x = stop_positions[start_stop]
        start_y = y_positions[seg_idx + 1]
        
        # Get packet count and color for diagonal connection
        diagonal_packet_count = diagonal_connections[seg_idx]
        if global_max > global_min:
            diagonal_norm_count = (diagonal_packet_count - global_min) / (global_max - global_min)
        else:
            diagonal_norm_count = 0.5
        
        color = cmap(diagonal_norm_count)
        
        # Draw diagonal connection with same style as regular connections
        diagonal_line = ax.plot([end_x, start_x], [end_y, start_y], '-', 
                              color=color, linewidth=4, alpha=0.7, zorder=1)[0]
        
        # Add static label at midpoint of diagonal connection
        mid_x = (end_x + start_x) / 2
        mid_y = (end_y + start_y) / 2
        label = ax.text(mid_x, mid_y - 0.3, f'{diagonal_packet_count:.0f}', 
                       ha='center', va='center', fontsize=8, 
                       color='black', zorder=2, visible=True)
        packet_labels.append(label)
    
    # LAYER 2: Plot stops on top (foreground layer)
    for seg_idx, segment in enumerate(segments):
        y_pos = y_positions[seg_idx]
        
        segment_x_positions = []
        segment_times = []
        
        for stop_data in segment['data']:
            x_pos = stop_positions[stop_data['stop']]
            segment_x_positions.append(x_pos)
            segment_times.append(stop_data['time_str'])
        
        packet_counts = segment['packet_counts']
        
        # Normalize packet counts for color mapping
        if global_max > global_min:
            normalized_counts = [(count - global_min) / (global_max - global_min) for count in packet_counts]
        else:
            normalized_counts = [0.5] * len(packet_counts)
        
        # Plot points with heatmap coloring and timestamps (high zorder for foreground)
        for j, (x_pos, norm_count, time_str) in enumerate(zip(segment_x_positions, normalized_counts, segment_times)):
            color = cmap(norm_count)
            point = ax.plot(x_pos, y_pos, 'o', color=color, markersize=16, alpha=0.9, zorder=10)[0]
            
            # Add time label above stop
            ax.text(x_pos, y_pos + 0.8, time_str, 
                   ha='center', va='bottom', fontsize=10, fontweight='bold',
                   rotation=0, color='black', zorder=11)
            
            # Add packet count label below stop
            label = ax.text(x_pos, y_pos - 0.8, f'{packet_counts[j]}', 
                           ha='center', va='top', fontsize=10, 
                           color='black', zorder=11, visible=True)
            packet_labels.append(label)
    
    # Add stop names at the bottom
    stop_label_y = min(y_positions) - 3.0
    for i, stop in enumerate(unique_stops):
        display_name = stop if len(stop) <= 35 else stop[:32] + "..."
        ax.text(i, stop_label_y, display_name, 
               ha='right', va='top', fontsize=10, fontweight='bold',
               rotation=45, rotation_mode='anchor')
    
    # Set limits - leave more space at bottom for button
    timestamp_top = max(y_positions) + 1.5
    stop_names_bottom = stop_label_y - 3.0  # More space for button
    ax.set_xlim(-0.5, len(unique_stops) - 0.5)
    ax.set_ylim(stop_names_bottom, timestamp_top)
    
    # Remove axis elements
    ax.set_xticks([])
    ax.set_yticks([])
    ax.spines['left'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['top'].set_visible(False)
    ax.spines['bottom'].set_visible(False)
    
    # Add title
    ax.set_title("Čtvrtek, Linka 9 - Hustota zachycených probe requestů", 
                fontsize=22, fontweight='bold', pad=25)
    
    # Get suggested ticks
    locator = MaxNLocator(nbins=4, integer=True)
    tick_values = locator.tick_values(global_min, global_max)

    # Clip to actual range
    tick_values = [val for val in tick_values if global_min <= val <= global_max]

    # Ensure min and max are included
    if global_min not in tick_values:
        tick_values.insert(0, global_min)
    if global_max not in tick_values:
        tick_values.append(global_max)

    # Sort and remove duplicates
    tick_values = sorted(set(tick_values))

    # Normalize and plot
    norm = plt.Normalize(vmin=global_min, vmax=global_max)
    mappable = plt.cm.ScalarMappable(norm=norm, cmap=cmap)
    mappable.set_array([])

    # Draw colorbar
    cbar_ax = fig.add_axes([0.25, 0.1, 0.5, 0.015])
    cbar = fig.colorbar(mappable, cax=cbar_ax, orientation='horizontal')
    cbar.set_label('Počet zachycených probe requestů', fontsize=12, fontweight='bold')
    cbar.set_ticks(tick_values)
    cbar.ax.set_xticklabels([f'{int(val)}' for val in tick_values])

    # Adjust layout to leave more space for everything
    plt.subplots_adjust(bottom=0.2, top=0.93, left=0.05, right=0.98)
    
    # Create toggle button with better positioning and event handling
    def toggle_labels(event):
        current_visibility = packet_labels[0].get_visible() if packet_labels else True
        for label in packet_labels:
            label.set_visible(not current_visibility)
        fig.canvas.draw_idle()  # Use draw_idle() instead of draw()
        print(f"Labels {'hidden' if current_visibility else 'shown'}")  # Debug feedback
    
    # Alternative: Add keyboard shortcut for toggling
    def on_key_press(event):
        if event.key == 't' or event.key == 'T':
            toggle_labels(event)
    
    fig.canvas.mpl_connect('key_press_event', on_key_press)
    
    print("- Press 'T' key to toggle labels")
    
    return [fig]

def main():
    """
    Main function with packet count heatmap visualization
    """
    print(f"Processing segments based on BREAK,BREAK entries...")
    
    # Detect segments based on breaks
    segments = detect_break_segments(df)
    
    print(f"Detected {len(segments)} segments")
    for segment in segments:
        print(f"Segment {segment['segment_id']}: {segment['start_stop']} -> {segment['end_stop']} ({len(segment['data'])} stops)")
    
    # Create unified heatmap visualization
    figures = plot_segments_unified_heatmap(segments, packet_df)
    
    # Show plots
    if figures:
        plt.show()
    
    return segments

# Run the analysis
if __name__ == "__main__":
    segments = main()