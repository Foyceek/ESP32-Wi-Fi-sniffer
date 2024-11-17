import pandas as pd
import matplotlib.pyplot as plt

# Read the CSV file containing MAC addresses
csv_file = r"C:\Franta\VUT\5_semestr\probe_analyzer\Python\source_data_prep\source_mac_addresses.csv"
df = pd.read_csv(csv_file)

# Convert all MAC addresses to uppercase
df['Source MAC Address'] = df['Source MAC Address'].str.upper()

# Function to categorize MAC addresses
def categorize_mac(mac):
    # Remove colons for consistency in comparison
    mac = mac.replace(":", "")
    
    # Check if the MAC address is Locally Assigned (DA:A1:19) prefix
    if len(mac) == 12:  # Ensure the MAC address is in the correct format (12 hex characters)
        # Check for the DA:A1:19 prefix
        if mac.startswith("DAA119"):
            return "Locally Assigned (DA:A1:19)"
        
        # Check if the MAC address is Locally Assigned based on second byte
        second_byte = mac[2]  # Get the second byte of the MAC address
        if second_byte in ['2', '6', 'A', 'E']:
            return "Locally Assigned"
    
    # If not, it's a Globally Unique MAC
    return "Globally Unique"

# Apply categorization to each MAC address
df['Category'] = df['Source MAC Address'].apply(categorize_mac)

# Count the occurrences of each category
category_counts = df['Category'].value_counts()

# Plot the results
plt.figure(figsize=(8, 6))
category_counts.plot(kind='bar', color=['blue', 'green', 'red'])
plt.title('MAC Address Categorization')
plt.xlabel('Category')
plt.ylabel('Count')
plt.xticks(rotation=45)
plt.tight_layout()

# Display the plot
plt.show()

# Optionally, print out the category counts for verification
print(category_counts)
