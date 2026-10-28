kimport struct
import os

FRAME_SIZE = 1024
RAW_FILE = 'badapple.raw'
OUT_FILE = 'badapple.bin'

def transpose_to_ssd1306(row_major_data):
    # Convert 128x64 (row-major) to SSD1306 format (page-major, vertical bit)
    col_major_data = bytearray(1024)
    for y in range(64):
        for x in range(128):
            # Read pixel from raw data (1 byte = 8 horizontal pixels)
            raw_byte = row_major_data[y * 16 + (x // 8)]
            bit = 7 - (x % 8) # ffmpeg monow writes the most significant bit to the left
            pixel = (raw_byte >> bit) & 1
            
            # Write to display format (1 byte = 8 vertical pixels)
            page = y // 8
            col = x
            new_idx = page * 128 + col
            bit_pos = y % 8
            
            if pixel:
                col_major_data[new_idx] |= (1 << bit_pos)
    return bytes(col_major_data)

def rle_compress(data):
    out = bytearray()
    i = 0
    while i < len(data):
        run_len = 1
        while i + run_len < len(data) and data[i] == data[i + run_len] and run_len < 127:
            run_len += 1
        
        if run_len >= 3:
            out.append(0x80 | run_len)
            out.append(data[i])
            i += run_len
        else:
            start = i
            while i < len(data) and (i - start) < 127:
                if i + 2 < len(data) and data[i] == data[i+1] == data[i+2]:
                    break
                i += 1
            out.append(i - start)
            out.extend(data[start:i])
    return bytes(out)

def main():
    if not os.path.exists(RAW_FILE):
        print(f"Error: {RAW_FILE} not found!")
        return

    with open(RAW_FILE, 'rb') as f:
        raw_data = f.read()

    total_frames = len(raw_data) // FRAME_SIZE
    print(f"Total frames: {total_frames}")

    compressed_frames = []
    for i in range(total_frames):
        frame = raw_data[i*FRAME_SIZE : (i+1)*FRAME_SIZE]
        transposed_frame = transpose_to_ssd1306(frame)
        compressed_frames.append(rle_compress(transposed_frame))

    with open(OUT_FILE, 'wb') as f:
        f.write(b'BAPP')
        f.write(struct.pack('<I', total_frames))
        
        current_offset = 0
        for cf in compressed_frames:
            f.write(struct.pack('<I', current_offset))
            current_offset += len(cf)
            
        for cf in compressed_frames:
            f.write(cf)
            
    print(f"Done! {OUT_FILE} size: {os.path.getsize(OUT_FILE) / 1024:.1f} KB")

if __name__ == '__main__':
    main()
