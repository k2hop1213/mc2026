"""시험 대비 도구 - 프로토콜 파서 스트레스 테스트

w05_protocol 의 패킷 형식(STX CMD LEN DATA SUM ETX)으로 올린 보드에
쓰레기 바이트, 잘린 패킷, 연속 패킷을 보내고 응답을 자동으로 검사한다.

실행 : python scripts/stress_test.py COM5
옵션 : --baud 9600, --max-data 16 (w05 코드는 --max-data 8)
보드에는 exam_base, exam_avr, w05_switch 처럼 ADC 요청(명령 0x05)을 받는 코드가 올라가 있어야 한다.
"""

import argparse
import sys
import time

from packet_tool import build_packet, serial, to_hex

ACK_ADC = 0x85
ERR = 0xFF
ERR_LEN = 1
ERR_SUM = 3
ERR_TIMEOUT = 4
ERR_DATA_LEN = 6

REQ = build_packet(0x05, b"\x01")  # 조도센서 값 요청


def parse(stream):
    """받은 바이트 -> [(cmd, data)]. 형식이 틀린 패킷은 ('BAD', 바이트)"""
    packets = []
    i = 0

    while i < len(stream):
        if stream[i] != 0x02 or i + 2 >= len(stream):
            packets.append(("BAD", stream[i:i + 1]))
            i += 1
            continue

        cmd, length = stream[i + 1], stream[i + 2]
        end = i + 3 + length + 2
        raw = stream[i:end]
        good = len(raw) == length + 5 and raw[-1] == 0x03 and sum(raw[1:-2]) & 0xFF == raw[-2]

        packets.append((cmd, bytes(raw[3:-2])) if good else ("BAD", raw))
        i = end

    return packets


class Board:
    def __init__(self, name, baud=9600):
        self.port = serial.Serial(name, baud, timeout=0.01)
        time.sleep(2.5)  # 포트를 열면 Uno 가 리셋된다
        self.port.reset_input_buffer()

    def send(self, data, wait=0.3):
        self.port.write(data)
        time.sleep(wait)

    def receive(self):
        return parse(self.port.read(4096))


def summary(packets):
    return [f"{p[0]:02X}" if p[0] != "BAD" else "BAD" for p in packets]


def errors(code, count=1):
    return [(ERR, bytes([code]))] * count


def is_adc(packet):
    return packet[0] == ACK_ADC and len(packet[1]) == 2


results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(f"[{'PASS' if ok else 'FAIL'}] {name} {detail}", flush=True)


def only_adc(name, board, count=1):
    packets = board.receive()
    ok = len(packets) == count and all(is_adc(p) for p in packets)
    check(name, ok, "" if ok else f"받음 {summary(packets)}")


def test_garbage(board):
    board.send(bytes([0xAA, 0x55, 0x00, 0xFF, 0x0D, 0x0A]) + REQ)
    only_adc("앞에 쓰레기 바이트", board)

    board.send(bytes([0x03, 0x03, 0x81]) + REQ)
    only_adc("앞에 ETX 값", board)


def test_truncated(board):
    for cut in range(1, len(REQ)):
        board.send(REQ[:cut], 0.2)
        board.send(REQ)
        packets = board.receive()
        ok = packets[:1] == errors(ERR_TIMEOUT) and len(packets) == 2 and is_adc(packets[1])
        check(f"{cut} 바이트에서 잘린 패킷 뒤 복귀", ok, "" if ok else f"받음 {summary(packets)}")


def test_corrupted(board):
    for i in range(1, len(REQ) - 1):
        bad = bytearray(REQ)
        bad[i] ^= 0xFF
        board.send(bytes(bad), 0.2)
        board.receive()  # 오류 종류는 위치마다 다르다
        board.send(REQ)
        only_adc(f"{i} 번째 바이트가 깨진 패킷 뒤 복귀", board)


def test_split(board):
    for b in REQ:
        board.send(bytes([b]), 0.05)
    time.sleep(0.2)
    only_adc("한 바이트씩 50ms 간격 전송", board)


def test_lengths(board, max_data):
    board.send(build_packet(0x05, bytes(max_data)))
    packets = board.receive()
    check(f"LEN {max_data} (버퍼 최대)", packets == errors(ERR_DATA_LEN), str(summary(packets)))

    board.send(build_packet(0x05, bytes(max_data + 1)))
    packets = board.receive()
    check(f"LEN {max_data + 1} (버퍼 초과)", packets == errors(ERR_LEN), str(summary(packets)))

    board.send(REQ)
    only_adc("LEN 초과 뒤 복귀", board)


def test_burst(board):
    board.send(REQ * 9, 0.5)
    only_adc("연속 패킷 9개 (54바이트)", board, 9)

    for _ in range(100):
        board.send(REQ, 0.03)
    time.sleep(0.5)
    only_adc("30ms 간격 패킷 100개", board, 100)


def test_limits(board):
    """통과 기준이 아니라 이 파서의 한계를 측정한다"""
    print("--- 한계 측정 ---", flush=True)

    board.send(bytes([0x02]) + REQ, 0.3)
    packets = board.receive()
    print(f"가짜 STX 뒤 정상 패킷 : {summary(packets)}", flush=True)
    time.sleep(0.2)

    board.send(build_packet(0x01, b"\x05"), 0.05)  # LED 3초 동작
    board.send(REQ * 20, 3.5)
    packets = board.receive()
    count = sum(1 for p in packets if is_adc(p))
    print(f"LED 동작 중 요청 20개(120바이트) : 응답 {count}개, 전체 {summary(packets)[:4]} ...", flush=True)


def main():
    parser = argparse.ArgumentParser(description="프로토콜 파서 스트레스 테스트")
    parser.add_argument("port", help="예: COM5")
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--max-data", type=int, default=16, help="보드 코드의 MAX_DATA")
    args = parser.parse_args()

    board = Board(args.port, args.baud)

    board.send(REQ)
    only_adc("정상 요청", board)

    test_garbage(board)
    test_truncated(board)
    test_corrupted(board)
    test_split(board)
    test_lengths(board, args.max_data)
    test_burst(board)
    test_limits(board)

    time.sleep(0.5)
    board.port.reset_input_buffer()
    board.send(REQ)
    only_adc("마지막 정상 요청", board)

    print(f"\n{sum(results)} / {len(results)} 통과", flush=True)
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
