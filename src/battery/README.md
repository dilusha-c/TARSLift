# Battery Monitoring Subsystem (`src/battery/`)

This module monitors power distribution, pack voltage, instantaneous current consumption, and estimated State-of-Charge (SoC %) to protect LiPo/LiFePO4 battery packs from deep discharge.

---

## 1. Hardware Architecture & Interfaces

The subsystem supports dual sensing methodologies:
1. **Digital I2C Power Monitor (INA219 @ `0x40`)**:
   - Measures bus voltage ($0-26\text{V}$) and differential voltage across a high-side current shunt resistor ($0.1\Omega$).
   - Computes real-time Current ($mA$) and Power ($mW$).
2. **Analog ADC Voltage Divider (GPIO 1)**:
   - Fallback sensing via two precision resistors ($R_1 = 10\text{k}\Omega, R_2 = 2.2\text{k}\Omega$):
     $$V_{\text{ADC}} = V_{\text{Battery}} \cdot \frac{R_2}{R_1 + R_2}$$
     $$V_{\text{Battery}} = V_{\text{ADC}} \cdot \left(\frac{R_1 + R_2}{R_2}\right)$$

---

## 2. Basic Theory & SoC Estimation

### State-of-Charge (SoC) Calculation
For a standard 3S Li-Ion/LiPo battery pack ($12.6\text{V}$ Fully Charged, $11.1\text{V}$ Nominal, $9.6\text{V}$ Empty/Cutoff):
$$\text{SoC} (\%) = \text{clamp}\left( \frac{V_{\text{pack}} - V_{\text{cutoff}}}{V_{\text{full}} - V_{\text{cutoff}}} \times 100\%, 0\%, 100\% \right)$$

### Protection Thresholds & Hysteresis
- **Warning Threshold ($10.8\text{V}$ / 20% SoC)**: Emits warning notifications to the Web Dashboard and error logs.
- **Critical Threshold ($10.2\text{V}$ / 10% SoC)**: Halts active repeat navigation missions, commands gentle deceleration, and prevents new autonomous mission dispatches.
- **Cutoff / E-Stop ($9.6\text{V}$)**: Shuts off motor PWM drivers immediately.

---

## 3. Key Functions & APIs (`battery_manager.h`)

- `void initBatteryManager()`: Initializes ADC calibration registers and I2C INA219 sensor.
- `void updateBattery()`: Periodic sampling function (called at 1Hz in FreeRTOS background).
- `BatteryData getBatteryData()`: Returns a struct with `voltage`, `current_mA`, `power_mW`, `percentage`, and `is_critical`.
