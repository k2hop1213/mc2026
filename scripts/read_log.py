"""시험 대비 도구 - 바이트 기록기(tool_byte_recorder)의 기록 읽기

실행
  기록 읽기 : python scripts/read_log.py COM5
  기록 지우기 : python scripts/read_log.py COM5 --clear
  옵션 : --baud 9600

조교 프로그램 연결을 끊고 실행한다. 100ms 이상 쉬면 다른 패킷(버튼)으로 보고 줄을 나눈다.
"""

import argparse
import sys
import time

from packet_tool import serial, to_hex, to_text

MAGIC = b"@@LOG"
GROUP_GAP = 10  # 10ms 단위


def read_until(port, words, timeout):
    """words 중 하나로 시작하는 줄이 나올 때까지 읽어 줄 목록을 돌려준다"""
    lines = []
    buffer = bytearray()
    end = time.monotonic() + timeout

    while time.monotonic() < end:
        buffer.extend(port.read(256))
        while b"\n" in buffer:
            line, _, rest = buffer.partition(b"\n")
            buffer = bytearray(rest)
            text = line.decode("ascii", "ignore").strip()
            lines.append(text)
            if text in words:
                return lines

    sys.exit("보드 응답이 없다. tool_byte_recorder 가 올라가 있고 보드레이트가 맞는지 확인한다.")


def remove_magic(entries):
    """읽기 명령 자신이 기록된 부분을 뺀다"""
    values = bytes(value for value, _ in entries)
    keep = [True] * len(entries)
    start = values.find(MAGIC)

    while start >= 0:
        for i in range(start, min(start + len(MAGIC) + 1, len(entries))):
            keep[i] = False
        start = values.find(MAGIC, start + 1)

    return [entry for entry, wanted in zip(entries, keep) if wanted]


def group(entries):
    groups = []

    for value, gap in entries:
        if not groups or gap >= GROUP_GAP:
            groups.append(bytearray())
        groups[-1].append(value)

    return groups


def main():
    parser = argparse.ArgumentParser(description="바이트 기록 읽기")
    parser.add_argument("port", help="예: COM5")
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--clear", action="store_true", help="기록 지우기")
    args = parser.parse_args()

    port = serial.Serial(args.port, args.baud, timeout=0.05)
    time.sleep(2.5)  # 포트를 열면 Uno 가 리셋된다
    port.reset_input_buffer()

    if args.clear:
        port.write(MAGIC + b"C")
        read_until(port, ("CLEARED",), 3)
        print("기록을 지웠다.")
        return

    port.write(MAGIC + b"D")
    lines = read_until(port, ("END",), 10)
    entries = []

    for line in lines:
        parts = line.split()
        if len(parts) == 2 and all(len(p) == 2 for p in parts):
            entries.append((int(parts[0], 16), int(parts[1], 16)))

    entries = remove_magic(entries)
    print(f"기록 {len(entries)} 바이트")

    for number, data in enumerate(group(entries), 1):
        print(f"{number:3d}: {to_hex(data)}   | {to_text(data)}")


if __name__ == "__main__":
    main()
