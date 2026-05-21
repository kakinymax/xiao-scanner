import struct
import csv
import sys

def parse_bin_to_csv(bin_file, csv_file):
    print(f"Reading {bin_file}...")
    with open(bin_file, 'rb') as f:
        data = f.read()
    
    print(f"File size: {len(data)} bytes")
    record_size = 12 # 3 floats * 4 bytes
    num_records = len(data) // record_size
    remainder = len(data) % record_size
    
    print(f"Found {num_records} complete records. (Remainder: {remainder} bytes ignored)")
    
    with open(csv_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Minute', 'Temperature', 'Humidity', 'DI'])
        
        for i in range(num_records):
            offset = i * record_size
            record = data[offset:offset+record_size]
            t, h, di = struct.unpack('<fff', record)
            writer.writerow([i, round(t, 2), round(h, 2), round(di, 2)])
            
    print(f"Successfully saved to {csv_file}")

if __name__ == '__main__':
    parse_bin_to_csv('logs/ai_env_log_1778673651.bin', 'logs/ai_env_data.csv')
