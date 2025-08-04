import asyncio
from evdev import UInput, ecodes as e, AbsInfo
from bleak import BleakClient, BleakScanner

TARGET_NAME = "Skylanders GamePad"
CHARACTERISTIC_UUID = "533e1541-3abe-f33f-cd00-594e8b0a8ea3"

# Create virtual gamepad with evdev + uinput
capabilities = {
    e.EV_KEY: [
        e.BTN_A, e.BTN_B, e.BTN_X, e.BTN_Y,
        e.BTN_TL, e.BTN_TR, e.BTN_TL2, e.BTN_TR2,
        e.BTN_DPAD_UP, e.BTN_DPAD_DOWN, e.BTN_DPAD_LEFT, e.BTN_DPAD_RIGHT,
        e.BTN_START # pause button = start button atm
    ],
    e.EV_ABS: [
        (e.ABS_X,  AbsInfo(0, -128, 127, 0, 0, 0)),
        (e.ABS_Y,  AbsInfo(0, -128, 127, 0, 0, 0)),
        (e.ABS_RX, AbsInfo(0, -128, 127, 0, 0, 0)),
        (e.ABS_RY, AbsInfo(0, -128, 127, 0, 0, 0)),
    ]
}

ui = UInput(events=capabilities, name="Skylanders GamePad", version=0x3)

# State tracking to avoid repeating inputs
state = {}

def emit_btn(code, pressed):
    if state.get(code) != pressed:
        ui.write(e.EV_KEY, code, int(pressed))
        state[code] = pressed

def emit_abs(code, value):
    if state.get(code) != value:
        ui.write(e.EV_ABS, code, value)
        state[code] = value

def decode_axis(val):
    """
    The gamepad uses a rather unorthodox axis encoding. Below will be explained in the context of the X axis (left/right), although the same principle applies to the Y axis.
    
    Moving from the center (0) to the right (127) works as expected, but moving to the left is encoded as 128 to 255. This means that the leftmost position is encoded as 127, and the position just before the center is 255.

    This will convert that to a value between -128 and 127, where 0 is the center, -128 is the leftmost position, and 127 is the rightmost position.
    """
    if val == 0:
        return 0
    elif val <= 127:
        return val
    else:
        return val - 256

def handle_notify(_, data: bytearray):
    # Buttons (value[8])
    buttons = data[8]
    emit_btn(e.BTN_DPAD_UP,    (buttons & 0x01) != 0)
    emit_btn(e.BTN_DPAD_DOWN,  (buttons & 0x02) != 0)
    emit_btn(e.BTN_DPAD_LEFT,  (buttons & 0x04) != 0)
    emit_btn(e.BTN_DPAD_RIGHT, (buttons & 0x08) != 0)
    emit_btn(e.BTN_A,          (buttons & 0x10) != 0)
    emit_btn(e.BTN_B,          (buttons & 0x20) != 0)
    emit_btn(e.BTN_X,          (buttons & 0x40) != 0)
    emit_btn(e.BTN_Y,          (buttons & 0x80) != 0)

    # Shoulder buttons (value[9])
    shoulders = data[9]
    emit_btn(e.BTN_TL, shoulders & 0x10)
    emit_btn(e.BTN_TR, shoulders & 0x20)

    # Triggers (value[10/11] == 0xFF means pressed)
    emit_btn(e.BTN_TL2, data[10] == 0xFF)
    emit_btn(e.BTN_TR2, data[11] == 0xFF)

    # Analog sticks
    emit_abs(e.ABS_RX, decode_axis(data[12]))
    emit_abs(e.ABS_RY, -decode_axis(data[13])) # Y-axis wants to be inverted ig
    emit_abs(e.ABS_X,  decode_axis(data[14]))
    emit_abs(e.ABS_Y,  -decode_axis(data[15]))

    # Pause button (we interpret this as the "start" button, this may change in the future to also be the "select" button through an alt fingering or smth)
    emit_btn(e.BTN_START, data[9] == 0x04)

    ui.syn()

async def main():
    print("Scanning...")
    devices = await BleakScanner.discover(timeout=5.0)
    device = next((d for d in devices if d.name == TARGET_NAME), None)

    if not device:
        print(f"Device '{TARGET_NAME}' not found.")
        return

    print(f"Connecting to {device.address}...")
    async with BleakClient(device) as client:
        print("Connected!")
        await client.start_notify(CHARACTERISTIC_UUID, handle_notify)

        print("Gamepad is live. Move sticks or press buttons! (Ctrl+C to stop)")
        while True:
            await asyncio.sleep(1)

try:
    asyncio.run(main())
finally:
    ui.close()
