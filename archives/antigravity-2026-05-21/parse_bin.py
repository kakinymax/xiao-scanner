import struct
import csv
import sys

# ヘッダー定数
AI_LOG_MAGIC = b'\xAE\x01'
AI_LOG_HEADER_SIZE = 16

def parse_bin_to_csv(bin_file, csv_file):
    print(f"Reading {bin_file}...")
    with open(bin_file, 'rb') as f:
        data = f.read()
    
    print(f"File size: {len(data)} bytes")
    
    # ヘッダー検出: 先頭2バイトがマジックナンバーなら新形式
    data_start = 0
    if len(data) >= AI_LOG_HEADER_SIZE and data[:2] == AI_LOG_MAGIC:
        version = struct.unpack('<H', data[2:4])[0]
        timestamp = struct.unpack('<I', data[4:8])[0]
        header_count = struct.unpack('<I', data[8:12])[0]
        print(f"Header detected: version={version}, timestamp={timestamp}, records={header_count}")
        data_start = AI_LOG_HEADER_SIZE
    else:
        print("No header detected (legacy format)")
    
    payload = data[data_start:]
    record_size = 12 # 3 floats * 4 bytes
    num_records = len(payload) // record_size
    remainder = len(payload) % record_size
    
    print(f"Found {num_records} complete records. (Remainder: {remainder} bytes ignored)")
    
    skipped = 0
    written = 0
    with open(csv_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Minute', 'Temperature', 'Humidity', 'DI'])
        
        for i in range(num_records):
            offset = i * record_size
            record = payload[offset:offset+record_size]
            t, h, di = struct.unpack('<fff', record)
            
            # 異常値フィルタリング: 温度 -20〜50℃、湿度 0〜100%
            if t < -20 or t > 50 or h < 0 or h > 100:
                skipped += 1
                continue
            
            writer.writerow([written, round(t, 2), round(h, 2), round(di, 2)])
            written += 1
    
    if skipped > 0:
        print(f"Warning: {skipped} records skipped due to abnormal values")
    print(f"Successfully saved {written} records to {csv_file}")

if __name__ == '__main__':
    parse_bin_to_csv('logs/ai_env_log_1778673651.bin', 'logs/ai_env_data.csv')

