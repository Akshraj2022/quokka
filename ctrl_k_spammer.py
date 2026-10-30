"""
Ctrl+K & Enter Spammer
=======================
- Press Right Alt (AltGr) to START spamming Ctrl+K then Enter.
- Move the mouse to STOP.
- Press Esc to quit the script entirely.

Requirements: pip install pynput pyautogui
"""

import threading
import time
import pyautogui
from pynput import keyboard, mouse

# --- Configuration ---
SPAM_DELAY = 0.15  # seconds between each Ctrl+K → Enter cycle

# --- State ---
spamming = False
stop_event = threading.Event()
quit_event = threading.Event()
mouse_pos = None  # tracks last known mouse position


def spam_loop():
    """Runs in a background thread; sends Ctrl+K then Enter while active."""
    global spamming
    while not quit_event.is_set():
        stop_event.wait()  # block until activated
        if quit_event.is_set():
            break
        print("[*] Spamming started  —  move the mouse to stop.")
        while stop_event.is_set() and not quit_event.is_set():
            pyautogui.hotkey("ctrl", "k")
            time.sleep(0.05)
            pyautogui.press("enter")
            time.sleep(SPAM_DELAY)
        print("[*] Spamming stopped.")


# ---- Keyboard listener ----
def on_key_press(key):
    global spamming, mouse_pos
    # Right Alt (AltGr) → toggle on
    if key == keyboard.Key.alt_gr:
        if not stop_event.is_set():
            # Record current mouse position as baseline
            mouse_pos = pyautogui.position()
            spamming = True
            stop_event.set()
    # Esc → quit everything
    if key == keyboard.Key.esc:
        print("[!] Esc pressed — exiting.")
        stop_event.clear()
        quit_event.set()
        return False  # stop keyboard listener


# ---- Mouse listener ----
MOVE_THRESHOLD = 5  # pixels of movement needed to count as "moved"

def on_move(x, y):
    global mouse_pos
    if stop_event.is_set() and mouse_pos is not None:
        dx = abs(x - mouse_pos[0])
        dy = abs(y - mouse_pos[1])
        if dx > MOVE_THRESHOLD or dy > MOVE_THRESHOLD:
            stop_event.clear()
            mouse_pos = None


# ---- Main ----
if __name__ == "__main__":
    print("=" * 50)
    print("  Ctrl+K  Enter Spammer")
    print("=" * 50)
    print("  Right Alt (AltGr)  → START spamming")
    print("  Move mouse         → STOP  spamming")
    print("  Esc                → QUIT  script")
    print("=" * 50)

    spam_thread = threading.Thread(target=spam_loop, daemon=True)
    spam_thread.start()

    mouse_listener = mouse.Listener(on_move=on_move)
    mouse_listener.start()

    # Keyboard listener runs on main thread (blocks until Esc)
    with keyboard.Listener(on_press=on_key_press) as kb_listener:
        kb_listener.join()

    # Cleanup
    quit_event.set()
    stop_event.set()  # unblock spam_loop so it can exit
    mouse_listener.stop()
    print("Bye!")
