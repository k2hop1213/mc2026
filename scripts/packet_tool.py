"""시험 대비 도구 - 2진 패킷 송수신기 (조교 UART 프로그램 대용)

시리얼 모니터로는 0x02 같은 값을 보낼 수 없어서 대신 쓴다.

실행
  포트 목록 : ~/.platformio/penv/Scripts/python.exe scripts/packet_tool.py
  연결      : ~/.platformio/penv/Scripts/python.exe scripts/packet_tool.py COM3
  옵션      : --baud 9600, --wait 2 (연결 후 보드 리셋을 기다리는 초)

입력 명령 (> 뒤에 입력)
  02 31 03      16진수 바이트를 그대로 보낸다
  t <13>        글자를 그대로 보낸다
  p 01 AA BB    패킷을 만들어 보낸다 : STX CMD LEN DATA... SUM ETX
                (01 = CMD, 나머지 = DATA, SUM = CMD + LEN + DATA 의 하위 8비트)
  pb 01 AA BB   p 와 같지만 체크섬을 일부러 틀리게 보낸다
  q             끝내기

업로드할 때는 이 프로그램을 끝낸다. (포트를 잡고 있으면 업로드가 실패한다)
"""

import argparse
import os
import subprocess
import sys
import threading
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    # pyserial 이 없거나 이름이 같은 다른 serial 패키지가 깔린 Python 이면
    # pyserial 이 들어 있는 PlatformIO 의 Python 으로 다시 실행한다
    penv = os.path.join(os.path.expanduser("~"), ".platformio", "penv")
    python = os.path.join(penv, "Scripts", "python.exe")
    if not os.path.exists(python):
        python = os.path.join(penv, "bin", "python")
    if not os.path.exists(python) or os.path.abspath(sys.executable) == os.path.abspath(python):
        sys.exit("pyserial 을 찾을 수 없다. PlatformIO 설치를 확인한다.")
    sys.exit(subprocess.call([python, os.path.abspath(sys.argv[0])] + sys.argv[1:]))

STX = 0x02
ETX = 0x03
IDLE_SEC = 0.05  # 이 시간 동안 안 오면 한 줄로 묶어 출력


def parse_hex(text):
    """'02 31 03' -> bytes"""
    return bytes(int(token, 16) for token in text.split())


def build_packet(cmd, data=b"", bad_sum=False):
    """STX CMD LEN DATA... SUM ETX"""
    if len(data) > 255:
        raise ValueError("DATA 는 255 바이트까지")

    body = bytes([cmd, len(data)]) + data
    checksum = sum(body) & 0xFF
    if bad_sum:
        checksum = (checksum + 1) & 0xFF

    return bytes([STX]) + body + bytes([checksum, ETX])


def to_hex(data):
    return " ".join(f"{b:02X}" for b in data)


def to_text(data):
    """출력 가능한 글자만 보여 주고 나머지는 '.'"""
    return "".join(chr(b) if 0x20 <= b < 0x7F else "." for b in data)


def show(tag, data):
    print(f"{tag} {to_hex(data)}   | {to_text(data)}", flush=True)


def reader(port, stop):
    """받은 바이트를 모아서 잠깐 끊기면 한 줄로 출력한다"""
    chunk = bytearray()
    last = time.monotonic()

    while not stop.is_set():
        try:
            data = port.read(port.in_waiting or 1)
        except (serial.SerialException, OSError) as e:
            if not stop.is_set():
                print(f"연결 끊김 : {e}", flush=True)
            return

        if data:
            chunk.extend(data)
            last = time.monotonic()
        elif chunk and time.monotonic() - last >= IDLE_SEC:
            show("RX", chunk)
            chunk.clear()

    if chunk:
        show("RX", chunk)


def make_payload(line):
    """입력 한 줄 -> 보낼 바이트. 빈 줄이면 None"""
    head, _, rest = line.strip().partition(" ")

    if not head:
        return None
    if head == "t":
        return rest.encode("ascii")
    if head in ("p", "pb"):
        values = parse_hex(rest)
        if not values:
            raise ValueError("CMD 가 필요하다. 예: p 01 AA")
        return build_packet(values[0], values[1:], bad_sum=head == "pb")
    return parse_hex(line)


def main():
    parser = argparse.ArgumentParser(description="2진 패킷 송수신기")
    parser.add_argument("port", nargs="?", help="예: COM3")
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--wait", type=float, default=2.0, help="연결 후 보드 리셋 대기(초)")
    args = parser.parse_args()

    if args.port is None:
        print("사용할 수 있는 포트 :")
        for info in list_ports.comports():
            print(f"  {info.device}  {info.description}")
        return

    try:
        port = serial.serial_for_url(args.port, baudrate=args.baud, timeout=0.01)
    except serial.SerialException as e:
        sys.exit(f"포트를 열 수 없다 (시리얼 모니터가 열려 있는지 확인) : {e}")

    # 포트를 열면 Uno 가 리셋된다. 부트로더가 끝날 때까지 기다린다
    time.sleep(args.wait)

    stop = threading.Event()
    thread = threading.Thread(target=reader, args=(port, stop), daemon=True)
    thread.start()

    print(f"{args.port} {args.baud}bps 연결됨. 예: 02 31 03 / t <13> / p 01 AA / q")

    try:
        for line in sys.stdin:
            if line.strip() == "q":
                break

            try:
                payload = make_payload(line)
            except ValueError as e:
                print(f"입력 오류 : {e}", flush=True)
                continue

            if payload:
                port.write(payload)
                show("TX", payload)
    except KeyboardInterrupt:
        pass

    time.sleep(IDLE_SEC * 4)  # 마지막 응답까지 출력
    stop.set()
    thread.join(timeout=1)
    port.close()


if __name__ == "__main__":
    main()
