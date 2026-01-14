import os
import subprocess
import time
from datetime import datetime

# ==========================================================
# CẤU HÌNH HỆ THỐNG NEXTFISH DATAGEN (TỐI ƯU KAGGLE)
# ==========================================================
CONFIG = {
    "REPO_URL": "https://github.com/AustraliaSilver/nextfish-data",
    "NODES": 8000,              # Chất lượng cao cho Nextfish
    "GAMES_PER_CORE": 400,      # Tổng ~1600 ván mỗi lần chạy 12 tiếng
    "THREADS": 4,               # Kaggle có 4 nhân CPU
    "HASH_MB": 128,             # RAM thoải mái trên Kaggle
    "OUTPUT_DIR": "/kaggle/working/nextfish_output",
    "BOOK_FILE": "book_moves.txt"
}

def run_command(cmd, cwd=None):
    """Hàm hỗ trợ chạy lệnh shell và in log trực tiếp"""
    print(f">> Đang chạy: {cmd}")
    process = subprocess.Popen(cmd, shell=True, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    for line in process.stdout:
        print(line, end="")
    process.wait()
    if process.returncode != 0:
        print(f"!! LỖI khi chạy lệnh: {cmd}")
        return False
    return True

def setup_environment():
    """Clone code, tải mạng nền và biên dịch engine"""
    if os.path.exists("nextfish-data"):
        run_command("rm -rf nextfish-data")
    
    print("\n--- 1. CLONE REPO & TẢI NET ---")
    run_command(f"git clone {CONFIG['REPO_URL']}")
    
    src_dir = "nextfish-data/src"
    # Tải 2 mạng Stockfish mạnh nhất để làm Soft-label
    run_command("wget https://tests.stockfishchess.org/api/nn/nn-c288c895ea92.nnue", cwd=src_dir)
    run_command("wget https://tests.stockfishchess.org/api/nn/nn-37f18f62d772.nnue", cwd=src_dir)

    print("\n--- 2. BIÊN DỊCH ENGINE (LINUX X86-64-AVX2) ---")
    # Tối ưu hóa cho CPU Intel/AMD trên Cloud
    compile_cmd = (
        "g++ -O3 -std=c++17 -Wall -fno-exceptions -DNDEBUG -DIS_64BIT -DUSE_PTHREADS -march=native "
        "benchmark.cpp bitboard.cpp evaluate.cpp main.cpp misc.cpp movegen.cpp movepick.cpp "
        "position.cpp search.cpp thread.cpp timeman.cpp tt.cpp uci.cpp ucioption.cpp "
        "tune.cpp syzygy/tbprobe.cpp nnue/nnue_accumulator.cpp nnue/nnue_misc.cpp "
        "nnue/network.cpp nnue/features/half_ka_v2_hm.cpp nnue/features/full_threats.cpp "
        "engine.cpp score.cpp memory.cpp nextfish_strategy.cpp nextfish_timeman.cpp "
        "datagen.cpp -o ../nextfish -lpthread -latomic"
    )
    run_command(compile_cmd, cwd=src_dir)
    run_command("chmod +x nextfish", cwd="nextfish-data")

    print("\n--- 3. TIỀN XỬ LÝ OPENING BOOK ---")
    run_command("python3 preprocess_pgn.py", cwd="nextfish-data")

def start_datagen():
    """Chạy song song 4 tiến trình trên 4 Core"""
    print(f"\n--- 4. BẮT ĐẦU TẠO DỮ LIỆU ({CONFIG['THREADS']} CORES) ---")
    if not os.path.exists(CONFIG['OUTPUT_DIR']):
        os.makedirs(CONFIG['OUTPUT_DIR'])
    
    processes = []
    for i in range(1, CONFIG['THREADS'] + 1):
        output_file = os.path.join(CONFIG['OUTPUT_DIR'], f"core{i}.binpack")
        # Lệnh datagen với các tham số tối ưu
        cmd = (
            f"./nextfish datagen nodes {CONFIG['NODES']} "
            f"games {CONFIG['GAMES_PER_CORE']} "
            f"book {CONFIG['BOOK_FILE']} "
            f"out {output_file}"
        )
        p = subprocess.Popen(cmd, shell=True, cwd="nextfish-data")
        processes.append(p)
        print(f"   [+] Core {i} đang chạy...")

    # Đợi tất cả hoàn thành (Khoảng 10-11 tiếng)
    for i, p in enumerate(processes):
        p.wait()
        print(f"   [-] Core {i+1} đã hoàn thành.")

def finalize_data():
    """Gộp dữ liệu và đặt tên theo thời gian"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    final_filename = f"nextfish_data_{timestamp}.binpack"
    final_path = os.path.join("/kaggle/working", final_filename)
    
    print("\n--- 5. GỘP DỮ LIỆU & DỌN DẸP ---")
    # Kiểm tra xem có file nào được tạo ra không
    if not os.listdir(CONFIG['OUTPUT_DIR']):
        print("!! Không tìm thấy dữ liệu nào để gộp.")
        return

    merge_cmd = f"cat {CONFIG['OUTPUT_DIR']}/*.binpack > {final_path}"
    run_command(merge_cmd)
    
    # Xóa file tạm
    run_command(f"rm -rf {CONFIG['OUTPUT_DIR']}")
    run_command("rm -rf nextfish-data")
    
    print("\n✅ THÀNH CÔNG!")
    print(f"Dữ liệu cuối cùng: {final_path}")
    print("Bạn có thể tải file này trong tab 'Output' sau khi Notebook chạy xong.")

# ==========================================================
# THỰC THI
# ==========================================================
if __name__ == "__main__":
    start_time = time.time()
    
    # Kaggle chạy trong môi trường /kaggle/working
    setup_environment()
    start_datagen()
    finalize_data()
    
    end_time = time.time()
    duration = (end_time - start_time) / 3600
    print(f"\n⏱️ Tổng thời gian thực hiện: {duration:.2f} giờ")
