"""시험 대비 도구 - 시험용 통합 틀(exam_base) 기능 테스트

파서 자체는 stress_test.py 로 검사하고, 여기서는 0x03 ~ 0x0B 명령과
LED 동작 중에도 명령을 처리하는지를 검사한다.

실행 : python scripts/exam_base_test.py COM5
LED 만 : python scripts/exam_base_test.py COM5 --led
LED 와 부저의 실제 동작은 눈과 귀로 확인한다. 보낼 때마다 볼 것을 출력한다.
"""

import sys
import time

from packet_tool import build_packet
from stress_test import Board, summary

ERR = 0xFF
ERR_CHANNEL = 7
ERR_DATA_LEN = 6
ACK = 0x80
ADC_EVENT = 0x91

results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(f"[{'PASS' if ok else 'FAIL'}] {name} {detail}", flush=True)


def request(board, cmd, data=b"", wait=0.2):
    board.send(build_packet(cmd, data), wait)
    return board.receive()


def expect_ack(board, name, cmd, data=b"", wait=0.2):
    packets = request(board, cmd, data, wait)
    check(name, packets == [(cmd | ACK, b"")], str(summary(packets)))


def test_auto(board):
    print("--- 자동 검사 ---", flush=True)
    expect_ack(board, "LED 점멸 시작", 0x01, b"\x05")
    packets = request(board, 0x05, b"\x00")
    check("LED 동작 중 ADC 응답", len(packets) == 1 and packets[0][0] == 0x85, str(summary(packets)))
    expect_ack(board, "동작 중 정지 명령", 0x0B)

    expect_ack(board, "주기 전송 시작 (100ms)", 0x09, bytes([1, 10]), 0.03)  # 첫 주기 패킷 전에 응답만 읽는다
    time.sleep(1.0)
    packets = board.receive()
    events = [p for p in packets if p[0] == ADC_EVENT]
    check("1초에 주기 패킷 약 10개", 9 <= len(events) <= 11, f"{len(events)}개")
    check("주기 패킷 형식", all(len(p[1]) == 3 and p[1][0] == 1 for p in events))

    expect_ack(board, "주기 전송 끄기", 0x09, bytes([1, 0]), 0.3)
    time.sleep(0.5)
    check("끈 뒤 패킷 없음", board.receive() == [])

    packets = request(board, 0x05, b"\x02")
    check("없는 채널", packets == [(ERR, bytes([ERR_CHANNEL]))], str(summary(packets)))
    packets = request(board, 0x07, b"\x01")
    check("부저 LEN 틀림", packets == [(ERR, bytes([ERR_DATA_LEN]))], str(summary(packets)))


def test_visible(board, led_only):
    print("--- 눈과 귀로 확인 ---", flush=True)
    steps = [
        ("LED 0x55 패턴", (0x03, b"\x55", 1.5)),
        ("LED 어두워짐", (0x06, b"\x20", 1.5)),
        ("LED 다시 밝아짐", (0x06, b"\xFF", 1.0)),
        ("왕복 시프트 1회", (0x04, b"\x01", 1.5)),
        ("가변저항 돌리면 LED 막대가 변함 (5초)", (0x0A, b"\x00", 5.0)),
    ]
    if not led_only:
        steps += [
            ("부저 1kHz 0.5초", (0x07, bytes([0x03, 0xE8, 0x01, 0xF4]), 1.0)),
            ("부저 도레미파솔라시도", (0x08, b"", 2.5)),
        ]

    for text, (cmd, data, wait) in steps:
        print(f">> {text}", flush=True)
        expect_ack(board, text, cmd, data, wait)

    expect_ack(board, "모두 정지", 0x0B)


def main():
    args = [arg for arg in sys.argv[1:] if arg != "--led"]
    led_only = "--led" in sys.argv

    if len(args) != 1:
        sys.exit("사용법 : python scripts/exam_base_test.py COM5 [--led]")

    board = Board(args[0])

    if not led_only:
        test_auto(board)
    test_visible(board, led_only)

    print(f"\n{sum(results)} / {len(results)} 통과", flush=True)
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
