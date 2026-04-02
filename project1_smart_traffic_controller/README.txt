Project 1 - Smart Traffic Light Controller

Arduino-based dual-intersection traffic controller with automatic timing and manual override.

Main features
- Vehicle detection with push buttons
- Non-blocking timing with millis()
- Dynamic runtime state (heap allocation)
- Serial status output and command menu

Project files
- arduino/smart_traffic_controller.ino
- docs/wiring_and_pcb_notes.txt
- docs/test_checklist.txt

Run in Tinkercad
1. Create a new Arduino Uno circuit.
2. Wire components using docs/wiring_and_pcb_notes.txt.
3. Paste arduino/smart_traffic_controller.ino into the code editor.
4. Start simulation.
5. Open Serial Monitor at 9600 baud.

Serial commands
- 1: show status
- 2: auto mode
- 3: manual mode (A green)
- 4: manual mode (B green)
- 5: clear counters
- 6: show help
- 7: toggle log interval (1000/2000/5000 ms)
- 8: show timing config
