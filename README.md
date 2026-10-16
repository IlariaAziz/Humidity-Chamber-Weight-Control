# Humidity Chamber – Weight Control

Firmware and acquisition software for a low-cost **gravimetric humidity chamber**:
a closed chamber in which relative humidity (RH) is kept at a set point while the
mass of a sample is recorded continuously with a load cell. It was built to follow
water uptake and release by materials under controlled humidity.

The chamber is based on an **STM32 Nucleo** board that reads **four DHT11**
temperature/humidity sensors and a **load cell** (through an **HX711** 24-bit ADC),
switches an **ultrasonic atomizer** and **two fans** to keep the RH at the set
point, and streams the readings over USB to a PC.

This repository contains:

- **test firmware** for each component (DHT11 sensors, load cell), with the
  sensor drivers;
- the **Python acquisition software** that plots the data live and saves it to CSV.

The main control firmware of the chamber is not included. Any firmware that
sends data in the [serial format](#serial-data-format) described below can be
used with the acquisition software.

---

## Repository structure

```
hardware/                      STM32 test firmware (C, STM32 HAL)
├── dht_11_4sensors/           Test program: 4 DHT11 sensors, machine-readable output
│   ├── main.c
│   ├── dht11.c
│   └── dht11.h                DHT11 driver
├── dht_11_single_sensor/      Test program: DHT11 sensors, human-readable output
│   ├── main.c
│   ├── dht11.c
│   └── dht11.h
└── load_cell_hx711/           Test/calibration program for the load cell
    ├── main.c
    ├── hx711.c
    ├── hx711.h                HX711 driver
    └── hx711Config.h
software/
└── plot_weight.py             Live plotting and CSV logging on the PC
pyproject.toml / uv.lock       Python dependencies
```

---

## Hardware

| Component | Role |
|---|---|
| STM32 Nucleo board (STM32F1 series, e.g. NUCLEO-F103RB) | Control and acquisition |
| 4 × DHT11 | Temperature and RH inside the chamber |
| Ultrasonic atomizer, driven through an optocoupler | Adds humidity |
| 2 × fans | Air circulation / drying |
| Load cell + HX711 module | Sample mass |

Each test program defines its pins at the top of its `main.c`.

---

## Firmware

The firmware was developed with **STM32CubeIDE** and the STM32 HAL library. The
folders contain the application sources only, not complete CubeIDE projects.

To build a test program:

1. Create a new STM32CubeIDE project for your Nucleo board.
2. Replace the generated `Core/Src/main.c` with the `main.c` of the program.
3. Copy its driver files into `Core/Src` (`.c`) and `Core/Inc` (`.h`).
4. Build and flash the board. The output is sent to the Nucleo's USB virtual
   COM port (USART2, 115200 baud) and can be viewed with any serial terminal.

### Test programs

- **`dht_11_4sensors`** – reads four DHT11 sensors and prints them in the same
  `name;T;RH#` format expected by the acquisition software. Useful to check wiring.
- **`dht_11_single_sensor`** – same, with human-readable output
  (`PA6 Temperature 25.3 Humidity 90.0`).
- **`load_cell_hx711`** – prints raw and 10-sample averaged HX711 readings,
  used to calibrate the load cell.

### Load-cell calibration

The mass is obtained from the raw HX711 value as
`mass (mg) = (raw − baseline) / slope`. Both values depend on the individual
load cell: with `load_cell_hx711`, record the averaged raw value with no load and
with a few reference masses, and fit a straight line. The intercept is the
baseline, the slope is in counts per mg.

### Serial data format

`plot_weight.py` expects the board to send one line per measurement cycle:

```
1234#s1;25.3;90.0#s2;24.8;89.0#s3;25.1;91.0#s4;25.0;90.0#
```

i.e. the mass in mg, then one `name;temperature (°C);humidity (%)#` block per
sensor, ending with a newline. A `|` between blocks is tolerated, and a block
with non-numeric values (e.g. `s2;ERROR;3`) is skipped. The `dht_11_4sensors`
program already prints the sensor blocks in this format; a complete firmware
only needs to print the mass and `#` first.

---

## Acquisition software

`software/plot_weight.py` reads the serial stream, plots mass and average RH live,
and saves all data to CSV. The code is commented in detail.

### Installation

Python ≥ 3.12 is required. With [uv](https://docs.astral.sh/uv/):

```bash
uv sync
```

or with pip:

```bash
pip install pyserial numpy pandas matplotlib
```

### Use

1. Flash firmware that sends the [serial format](#serial-data-format) above.
2. Connect the Nucleo board by USB and find its COM port (Windows Device Manager → *Ports (COM & LPT)*).
3. Set `SERIAL_PORT` (and, if you like, `FILE_LABEL`) at the top of `plot_weight.py`.
4. Run:

   ```bash
   uv run software/plot_weight.py
   # or
   python software/plot_weight.py
   ```

5. Stop the acquisition by pressing **`q`** in the plot window, closing the
   window, or pressing **Enter** in the console. The data is then saved.

The script runs on **Windows only**, because it uses `msvcrt` to detect the Enter key.

### Output

A file `YYYYmmdd_HHMMSS_<FILE_LABEL>.csv` in the working directory, with columns:

| Column | Unit |
|---|---|
| `time (s)` | seconds since the start (PC clock) |
| `weight (mg)` | mass as sent by the board |
| `avg humidity (RH%)` | mean RH of the sensors that read correctly in that cycle |
| `sX_temperature (°C)`, `sX_humidity (RH%)` | individual readings of each sensor (NaN if missing) |

No filtering or smoothing is applied to the data.

---

## Performance and limitations

- **DHT11 sensors:** ±5 % RH and ±2 °C accuracy, 1 % RH and 1 °C resolution;
  specified range 20–90 % RH and 0–50 °C. Above ~90 % RH individual sensors can
  saturate at their maximum reading (99 %), and sensors differ by a few % RH from
  each other. Cross-checking against a reference hygrometer is recommended.
- **Failed sensor reads:** if the firmware does not check the return value of
  `DHT11_Read()`, a sensor that stops responding keeps reporting its last valid
  value. Check the per-sensor columns in the CSV for values that never change.
- **Load cell:** the HX711 runs at 10 samples/s (when its RATE pin is grounded,
  as on most breakout modules) and each logged point is a single conversion. In
  our setup the standard deviation of repeated readings under a constant load
  of ~2.1 kg was ≈ 12 mg.
- **Temperature:** the chamber is not temperature-controlled. Changes in room
  temperature affect both the RH and the load-cell reading.

---

## Citation

If you use this setup, please cite:

> *[Authors, title, journal, year, DOI – to be added]*

## License

Released under the MIT License – see [`LICENSE`](LICENSE).
