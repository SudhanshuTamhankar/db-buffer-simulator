#!/bin/bash
# Create the header for your CSV file
echo "Workload,Strategy,LogicalReads,PhysicalReads,PhysicalWrites" > data.csv

echo "Running tests..."

# --- Balanced Test (testpf) ---
echo "Running: Balanced (LRU)"
rm -f file1 file2 # Cleanup
echo "1" | ./testpf -q >> data.csv

echo "Running: Balanced (MRU)"
rm -f file1 file2 # Cleanup
echo "2" | ./testpf -q >> data.csv

# --- Read-Heavy Test ---
echo "Running: Read-Heavy (LRU)"
rm -f read_heavy_file # Cleanup
echo "1" | ./test_read_heavy -q >> data.csv

echo "Running: Read-Heavy (MRU)"
rm -f read_heavy_file # Cleanup
echo "2" | ./test_read_heavy -q >> data.csv

# --- Write-Heavy Test ---
echo "Running: Write-Heavy (LRU)"
rm -f write_heavy_file # Cleanup
echo "1" | ./test_write_heavy -q >> data.csv

echo "Running: Write-Heavy (MRU)"
rm -f write_heavy_file # Cleanup
echo "2" | ./test_write_heavy -q >> data.csv

# --- Cyclic Access Test (shows LRU vs MRU difference) ---
echo "Running: Cyclic (LRU)"
rm -f cyclic_file # Cleanup
echo "0" | ./test_cyclic -q >> data.csv

echo "Running: Cyclic (MRU)"
rm -f cyclic_file # Cleanup
echo "1" | ./test_cyclic -q >> data.csv

echo "Done. Your data is in data.csv"
cat data.csv