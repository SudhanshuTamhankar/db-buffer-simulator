import pandas as pd
import matplotlib.pyplot as plt

# --- 1. Read the CSV data ---
try:
    data = pd.read_csv("data.csv")
except FileNotFoundError:
    print("Error: data.csv not found.")
    print("Please run ./run_all.sh first.")
    exit()

# --- 2. Prepare the data for graphing ---
# Group the data by 'Workload' and 'Strategy'
grouped_data = data.pivot(index='Workload', columns='Strategy', values=['PhysicalReads', 'PhysicalWrites', 'LogicalReads'])

# --- 3. Plot Physical I/O (Reads + Writes) ---
# We stack PhysicalReads and PhysicalWrites to show total disk I/O
physical_io = data.pivot(index='Workload', columns='Strategy', values=['PhysicalReads', 'PhysicalWrites'])
physical_io.plot(
    kind='bar',
    stacked=True,
    title='Physical I/O (Reads + Writes) by Workload',
    rot=0 # Rotation of x-axis labels
)
plt.ylabel("Total Physical I/Os")
plt.tight_layout()
plt.savefig("physical_io_graph.png")
print("Graph saved to physical_io_graph.png")

# --- 4. Plot Logical Reads (Cache Hits) ---
logical_io = data.pivot(index='Workload', columns='Strategy', values='LogicalReads')
logical_io.plot(
    kind='bar',
    title='Logical Reads (Cache Hits) by Workload',
    rot=0
)
plt.ylabel("Total Logical Reads")
plt.tight_layout()
plt.savefig("logical_reads_graph.png")
print("Graph saved to logical_reads_graph.png")

print("\nAll graphs created successfully!")