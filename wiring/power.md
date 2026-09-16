# Complete Power System Integration Guide

### Components List
* **Battery:** 3.7V 3800mAh Li-ion / Li-Po Cell
* **Charger:** TP4056 Module (with DW01A protection IC)
* **Booster:** MT3608 DC-DC Step-Up Converter
* **Indicators:** 1x Red 3mm/5mm LED, 1x Blue (or Green) 3mm/5mm LED, 2x 1kΩ Resistors (1/4W)
* **Loads:** ESP32 Development Board, 5V SD Card Module
* *(Optional but recommended):* SPST Toggle/Slide Switch (for system ON/OFF)

---

## Complete Master Wiring Diagram

```text
               +-------------------------------------------------------------------+
               |                                                                   |
               |                     [ + ] Input Pad (Top Left)                    |
               |                               |                                   |
               |                 +-------------+-------------+                     |
               |                 |                           |                     |
               |           [1kΩ Resistor]              [1kΩ Resistor]              |
               |                 |                           |                     |
               |             (Anode +)                   (Anode +)                 |
               |             RED LED                     BLUE LED                  |
               |            (Cathode -)                 (Cathode -)                |
               |                 |                           |                     |
               |                 v                           v                     |
               |            TP4056 Pin 7                TP4056 Pin 6               |
               |          (Top: 2nd from L)           (Top: 3rd from L)            |
               |                                                                   |
+------------+ |  +-----------------------+          +-------------------+         |  +-------------------+
|            | |  |     TP4056 MODULE     |          |  MT3608 BOOSTER   |         |  |      LOADS        |
|  3.7V      | |  |                       |          |                   |         |  |                   |
|  3800mAh   +-+->| B+                OUT+|==[Switch]==> VIN+      VOUT+ |====5.0V====> ESP32 'VIN' (5V)  |
|  BATTERY   |    |                       |          |          (Adjusted)         +==> SD Module 'VCC'   |
|            |    |                       |          |                   |         |                      |
|            +--->| B-                OUT-|=========>| VIN-        VOUT- |====GND=====> ESP32 'GND'        |
+------------+    +-----------------------+          +-------------------+         +==> SD Module 'GND'   |
                                                                                   +-------------------+
```

---

## Phase 1: MT3608 Boost Calibration (DO THIS FIRST)
*MT3608 boards ship from the factory set to random voltages (frequently up to 28V). You must calibrate it before connecting the ESP32.*

1. Solder two temporary wires from TP4056 **`OUT+`** and **`OUT-`** to MT3608 **`VIN+`** and **`VIN-`**.
2. Connect your battery to TP4056 **`B+`** and **`B-`**.
3. Set your digital multimeter to **DC Voltage** mode and place the probes on MT3608 **`VOUT+`** and **`VOUT-`**.
4. Using a small flathead screwdriver, turn the brass potentiometer screw **counter-clockwise**.
   * *Note: You may need to rotate it 10–15 full turns before the voltage suddenly begins to drop.*
5. Adjust until the multimeter reads steadily between **`5.0V` and `5.1V`**.
6. Disconnect the battery while completing the rest of the wiring.

---

## Phase 2: Step-by-Step Assembly

### Step 1: External Charging Indicators (Non-Destructive)
*(Hold the TP4056 with the USB port pointing to the left, as shown in your photo).*

1. **Prepare the Resistors:**
   * Solder one **1kΩ resistor** to the long leg (**Anode / +**) of the **Red LED**.
   * Solder the second **1kΩ resistor** to the long leg (**Anode / +**) of the **Blue LED**.
2. **5V Common Rail:**
   * Join the loose ends of both resistors together.
   * Solder them to the large copper **`+` pad** located at the top-left edge of the board (next to the USB port).
3. **Red LED (Charging):**
   * Solder a wire from the short leg (**Cathode / -**) of the **Red LED** to **Leg 2** of the TP4056 IC (the second pin from the left on the top row).
4. **Blue LED (Full):**
   * Solder a wire from the short leg (**Cathode / -**) of the **Blue LED** to **Leg 3** of the TP4056 IC (the third pin from the left on the top row).

---

### Step 2: Connect the Battery
* **Battery Red Lead (+):** Solder to the TP4056 pad labeled **`B+`**.
* **Battery Black Lead (-):** Solder to the TP4056 pad labeled **`B-`**.

> **Protection Note:** Never wire the battery directly to the booster. Going through `B+` and `B-` routes your battery through the onboard DW01A protection IC, which automatically cuts the circuit if the battery drops below 2.4V or experiences an overcurrent spike.

---

### Step 3: Connect Charger to Booster (With Power Switch)
* Solder TP4056 **`OUT-`** directly to MT3608 **`VIN-`**.
* Solder TP4056 **`OUT+`** to one terminal of your **ON/OFF Switch**.
* Solder the other terminal of the switch to MT3608 **`VIN+`**.
  *(If you are not using a switch, wire `OUT+` directly to `VIN+`).*

---

### Step 4: Powering the ESP32 and SD Module
* Connect MT3608 **`VOUT+`** (tuned to 5V) to:
  * ESP32 **`VIN`** (labeled **`5V`** on some ESP boards).
  * SD Card Module **`VCC`**.
* Connect MT3608 **`VOUT-`** (GND) to:
  * ESP32 **`GND`**.
  * SD Card Module **`GND`**.

---

## Operational Behavior Checklist

| Scenario | Built-in LEDs | External LEDs | ESP32 / SD Card State |
| :--- | :--- | :--- | :--- |
| **USB Plugged In & Charging** | Internal RED is ON | External RED is ON | Running normally / Charging |
| **USB Plugged In & Battery Full** | Internal BLUE is ON | External BLUE is ON | Running normally / Charge Complete |
| **Running on Battery (Unplugged)**| Both are OFF | Both are OFF | Powered purely by Battery |
| **Switch Turned OFF** | Shows charge status if plugged in | Shows charge status if plugged in | Completely unpowered (0mA draw) |

---

## Final Safety Rules
1. **Never use the ESP32 `3V3` pin for power input:** Always feed your stepped-up 5V to the `VIN` or `5V` pin. The onboard regulator steps it down safely to the 3.3V the ESP32 chip requires.
2. **Uploading Code to ESP32:** When you plug your computer's USB cable into the ESP32 to flash code, **turn your hardware power switch OFF**. This prevents two power supplies (PC USB and MT3608) from competing on the same 5V line.
