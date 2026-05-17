import serial
import wave
import struct
import time

# --- CẤU HÌNH ---
PORT = 'COM3'  # Thay đổi thành cổng COM của ESP32 (xem trong Arduino IDE)
BAUD = 921600  # Phải khớp với Serial.begin trong code ESP32
FILENAME = "record_voice.wav"
DURATION_SECONDS = 5  # Thời gian thu âm mỗi lần
SAMPLE_RATE = 16000   # Tần số lấy mẫu (phải khớp với ESP32)

def record_audio():
    try:
        # Mở cổng Serial
        ser = serial.Serial(PORT, BAUD, timeout=1)
        print(f"--- Đang kết nối tới {PORT} ---")
        time.sleep(2) # Đợi Serial ổn định
        ser.flushInput()

        frames = []
        total_samples = SAMPLE_RATE * DURATION_SECONDS
        
        print(f"--- ĐANG THU ÂM TRONG {DURATION_SECONDS} GIÂY ---")
        print("Hãy nói vào micro ngay bây giờ!")

        count = 0
        while count < total_samples:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            
            try:
                # Chuyển đổi dòng text sang số nguyên 16-bit
                sample = int(line)
                
                # Giới hạn giá trị trong dải int16 (-32768 đến 32767)
                sample = max(-32768, min(32767, sample))
                
                # Đóng gói dữ liệu dạng Little-endian 16-bit
                frames.append(struct.pack('<h', sample))
                
                count += 1
                
                # Hiển thị tiến độ mỗi 10%
                if count % (total_samples // 10) == 0:
                    print(f"Tiến độ: {int(count/total_samples*100)}%")
                    
            except ValueError:
                # Bỏ qua các dòng không phải số (ví dụ các câu in debug)
                continue

        ser.close()

        # Lưu dữ liệu vào file WAV
        print(f"--- Đang lưu vào file: {FILENAME} ---")
        with wave.open(FILENAME, 'wb') as wf:
            wf.setnchannels(1)      # Mono
            wf.setsampwidth(2)      # 2 bytes (16-bit)
            wf.setframerate(SAMPLE_RATE)
            wf.writeframes(b''.join(frames))
            
        print("--- THÀNH CÔNG! ---")
        print(f"Bạn có thể mở file '{FILENAME}' để nghe lại.")

    except serial.SerialException as e:
        print(f"Lỗi cổng Serial: {e}")
        print("Hãy đảm bảo bạn đã TẮT Serial Monitor trong Arduino IDE.")
    except Exception as e:
        print(f"Lỗi không xác định: {e}")

if __name__ == "__main__":
    record_audio()
