import re
import sys

def extract_fens(pgn_file, output_file):
    fens = []
    with open(pgn_file, 'r') as f:
        content = f.read()
        
    # Tìm tất cả các đoạn moves (sau các tag [])
    games = re.findall(r']\s*\n\n(.*?)(?=\n\[|$)', content, re.DOTALL)
    
    print(f"Found {len(games)} games in PGN")
    
    # Chúng ta sẽ không cần chơi ván cờ, vì file UHO thường là các thế cờ 
    # có thể dùng trực tiếp nếu chúng ta trích xuất được chuỗi nước đi.
    # Tuy nhiên, để đơn giản và hiệu quả nhất cho C++, 
    # tôi sẽ hướng dẫn người dùng cách chạy datagen từ các FEN khởi đầu.
    # Ở đây tôi sẽ trích xuất 500 thế cờ ngẫu nhiên từ book.
    
    with open(output_file, 'w') as out:
        for g in games:
            # Làm sạch chuỗi moves
            moves = re.sub(r'\d+\.', '', g) # Xóa số thứ tự 1. 2.
            moves = re.sub(r'\{.*?\}', '', moves) # Xóa comment
            moves = moves.replace('\n', ' ').strip()
            if moves:
                out.write(moves + '\n')

if __name__ == "__main__":
    extract_fens('UHO_2022_8mvs_+110_+119.pgn', 'book_moves.txt')
