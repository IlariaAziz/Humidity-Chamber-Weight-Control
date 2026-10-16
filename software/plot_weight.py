"""
plot_weight.py
==============

Real-time acquisition, plotting and logging of mass and relative humidity
from a gravimetric humidity chamber.

The chamber is controlled by an STM32 Nucleo board that reads:
  * one load cell through an HX711 24-bit ADC (mass, in mg), and
  * four DHT11 sensors (temperature in °C and relative humidity in % RH).

The board sends one line of text over USB serial per measurement cycle
(~2.5 s). This script:
  1. reads those lines in a background thread,
  2. parses the mass and the per-sensor temperature/humidity,
  3. plots mass and average humidity live on two y-axes,
  4. saves everything to a timestamped CSV file when the acquisition stops.

Expected serial line format
---------------------------
    <mass>#<id>;<T>;<RH>#<id>;<T>;<RH>#...

for example:

    1234#s1;25.3;90.0#s2;24.8;89.0#s3;25.1;91.0#s4;25.0;90.0#

  * <mass>  : mass in mg (already offset- and calibration-corrected on the board)
  * <id>    : sensor name (s1 ... s4)
  * <T>     : temperature in °C
  * <RH>    : relative humidity in %
Fields are separated by ';', sensor blocks by '#'. A '|' between blocks and a
trailing '|' are tolerated. A sensor block whose values are not numeric
(e.g. "s2;ERROR;3", sent when a read fails) is skipped for that cycle.

No filtering or smoothing is applied here: the mass is logged exactly as sent
by the board, and the average humidity is the plain mean of the sensors that
returned a valid reading in that cycle.

Stopping the acquisition
------------------------
Any of the following stops the acquisition and saves the CSV:
  * press 'q' with the plot window focused,
  * close the plot window,
  * press Enter in the console window.

Output
------
A CSV file named  <YYYYmmdd_HHMMSS>_<FILE_LABEL>.csv  in the working directory,
with columns:
    time (s), weight (mg), avg humidity (RH%),
    s1_temperature (°C), s1_humidity (RH%), ..., s4_humidity (RH%)
Time is measured on the PC, in seconds since the script started. Missing or
failed sensor readings are stored as NaN.

Requirements
------------
Python 3, pyserial, numpy, pandas, matplotlib.
Windows only (uses msvcrt to detect the Enter key in the console).
"""

import serial                      # pyserial: serial communication with the board
import matplotlib.pyplot as plt    # live plotting
import numpy as np                 # averaging and NaN handling
import time                        # timestamps and file naming
import pandas as pd                # building and saving the CSV
from queue import Queue            # thread-safe hand-off between reader and plotter
import threading                   # background thread for serial reading
import msvcrt                      # Windows-only: non-blocking keyboard check in the console


# ---------------------------------------------------------------------------
# User settings
# ---------------------------------------------------------------------------
SERIAL_PORT = 'COM6'       # serial port of the Nucleo board (check Device Manager)
BAUD_RATE = 115200         # must match the UART baud rate set in the firmware
FILE_LABEL = 'weight'      # descriptive label appended to the output file name
SENSOR_NAMES = ['s1', 's2', 's3', 's4']   # sensor IDs as sent by the firmware


# ---------------------------------------------------------------------------
# Serial connection and data storage
# ---------------------------------------------------------------------------
# timeout=1 makes readline() return after 1 s even if no full line arrived,
# so the reader thread never blocks forever.
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

# Lists that grow by one element per measurement cycle
weight_data = []      # mass (mg)
humidity_data = []    # average RH over the valid sensors (%)
time_data = []        # time since start (s), taken on the PC

# Per-sensor histories, e.g. sensor_humidity_data['s1'] = [90.0, 91.0, ...]
sensor_names = SENSOR_NAMES
sensor_temperature_data = {name: [] for name in sensor_names}
sensor_humidity_data = {name: [] for name in sensor_names}

start_time = time.time()   # reference time for the 'time (s)' column

# The reader thread puts parsed cycles in this queue; the main thread takes
# them out to store and plot. Queue is thread-safe, so no locks are needed.
data_queue = Queue()


# ---------------------------------------------------------------------------
# Live plot setup
# ---------------------------------------------------------------------------
plt.ion()   # interactive mode: the figure updates without blocking the script
fig, ax1 = plt.subplots(figsize=(10, 6))

# Second y-axis sharing the same x-axis: mass on the left, humidity on the right
ax2 = ax1.twinx()

# Event used to signal "stop and save" from the plot window callbacks
stop_event = threading.Event()


def handle_close(event):
    """Stop the acquisition when the plot window is closed."""
    stop_event.set()


def on_key(event):
    """Stop the acquisition when 'q' is pressed with the plot window focused."""
    if event.key == 'q':
        stop_event.set()


# Connect the callbacks to the figure
fig.canvas.mpl_connect('close_event', handle_close)
fig.canvas.mpl_connect('key_press_event', on_key)

# Empty line objects; their data is replaced at every update
line1, = ax1.plot([], [], 'b-', label='Weight')     # mass, left axis (blue)
line2, = ax2.plot([], [], 'g-', label='Humidity')   # avg RH, right axis (green)

# Axis labels
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Weight (mg)')
ax2.set_ylabel('Humidity (%)')

# A single legend listing the lines from both axes
lines = [line1, line2]
labels = [l.get_label() for l in lines]
ax1.legend(lines, labels, loc='upper left')

# Reminder box in the lower-left corner of the plot
instructions_text = (
    "Press 'q' to stop and save.\n"
)
ax1.text(
    0.01,
    0.02,
    instructions_text,
    transform=ax1.transAxes,   # coordinates relative to the axes (0-1)
    fontsize=9,
    va='bottom',
    ha='left',
    bbox=dict(boxstyle='round', facecolor='w', alpha=0.7, edgecolor='gray')
)


def set_dynamic_ylim(axis, values, padding_ratio=0.05, min_padding=1.0):
    """
    Rescale the y-axis to fit all data collected so far, plus some margin.

    The margin is 5% of the data range (padding_ratio), but never less than
    min_padding, so a flat signal is not drawn squashed against the axis edge.
    """
    if not values:
        return

    vmin = min(values)
    vmax = max(values)

    if vmin == vmax:
        # Constant signal: base the padding on the value itself
        padding = max(min_padding, abs(vmin) * padding_ratio)
    else:
        padding = max(min_padding, (vmax - vmin) * padding_ratio)

    axis.set_ylim(vmin - padding, vmax + padding)


# ---------------------------------------------------------------------------
# Serial reader (runs in a background thread)
# ---------------------------------------------------------------------------
def read_serial():
    """
    Continuously read lines from the serial port, parse them and put the
    result in data_queue as (weight, avg_humidity, sensor_snapshot).

    sensor_snapshot is a dict such as
        {'s1': {'temperature': 25.3, 'humidity': 90.0}, 's2': {...}, ...}
    containing only the sensors that sent valid numbers in this cycle.

    Running in its own thread keeps the serial buffer emptied even while the
    main thread is busy redrawing the plot.
    """
    while True:
        # Read one line, decode it and strip spaces and the trailing \r\n.
        # errors='ignore' drops any corrupted bytes instead of crashing.
        serial_data = ser.readline().decode('utf-8', errors='ignore').strip()
        if not serial_data:
            continue   # timeout with no data, or empty line

        try:
            # Drop a trailing '|' separator, if present
            if serial_data.endswith('|'):
                serial_data = serial_data[:-1]

            # Split into blocks on '#', discarding empty blocks.
            # segments[0] is the mass, the rest are sensor blocks.
            segments = [segment for segment in serial_data.split('#') if segment]
            if len(segments) < 2:
                continue   # no sensor data in this line: skip it

            weight = float(segments[0])   # mass in mg
            sensor_snapshot = {}

            for segment in segments[1:]:
                parts = segment.split(';')   # -> [id, T, RH]
                if len(parts) < 3:
                    continue   # incomplete block
                # Normalise the sensor ID: remove spaces and any '|' separator
                sensor_id = parts[0].strip(' |').lower()
                try:
                    temperature = float(parts[1])
                    humidity = float(parts[2])
                except ValueError:
                    # Non-numeric values (e.g. an "ERROR" block): skip this sensor
                    continue
                sensor_snapshot[sensor_id] = {
                    'temperature': temperature,
                    'humidity': humidity
                }

            if not sensor_snapshot:
                continue   # no valid sensor in this line: skip it

            # Plain mean of the humidity over the sensors that read correctly
            # in this cycle (failed sensors are excluded, not counted as 0)
            avg_humidity = np.mean(
                [values['humidity'] for values in sensor_snapshot.values()]
            )

            data_queue.put((weight, avg_humidity, sensor_snapshot))
        except ValueError as e:
            # The mass field could not be converted: report and skip the line
            print(f"Error parsing data: {e} | raw: {serial_data}")


# ---------------------------------------------------------------------------
# Main acquisition loop
# ---------------------------------------------------------------------------
try:
    # Start the reader thread. daemon=True: it is killed automatically when
    # the main program ends, so it never keeps the script alive.
    thread = threading.Thread(target=read_serial, daemon=True)
    thread.start()
    print("Data collection started. Close the plot window, press 'q' in the figure, or press Enter to stop...")

    while True:
        # Stop if the window was closed or 'q' was pressed
        if stop_event.is_set() or not plt.fignum_exists(fig.number):
            break

        if not data_queue.empty():
            weight, avg_humidity, sensor_snapshot = data_queue.get()

            # Timestamp on the PC, in seconds since the script started.
            # It is taken when the data is plotted, which lags the actual
            # measurement by at most a few hundred ms.
            current_time = time.time() - start_time
            time_data.append(current_time)
            weight_data.append(weight)
            humidity_data.append(avg_humidity)

            # Store each sensor's reading; NaN marks a missing/failed reading,
            # so all columns keep the same length
            for name in sensor_names:
                readings = sensor_snapshot.get(name)
                if readings:
                    sensor_temperature_data[name].append(readings['temperature'])
                    sensor_humidity_data[name].append(readings['humidity'])
                else:
                    sensor_temperature_data[name].append(np.nan)
                    sensor_humidity_data[name].append(np.nan)

            # Update the plotted lines with the full history
            line1.set_xdata(time_data)
            line1.set_ydata(weight_data)
            line2.set_xdata(time_data)
            line2.set_ydata(humidity_data)

            # Rescale the axes to the data collected so far
            ax1.set_xlim(0, current_time if current_time > 1 else 1)
            set_dynamic_ylim(ax1, weight_data)
            set_dynamic_ylim(ax2, humidity_data)

            plt.draw()
            plt.pause(0.1)   # let matplotlib process the redraw and GUI events

        # Stop if Enter was pressed in the console (Windows only)
        if msvcrt.kbhit():
            if msvcrt.getch() == b'\r':
                break
        plt.pause(0.1)   # keep the GUI responsive and avoid a busy loop

    # -----------------------------------------------------------------------
    # Save all collected data to CSV
    # -----------------------------------------------------------------------
    data = {
        'time (s)': time_data,
        'weight (mg)': weight_data,
        'avg humidity (RH%)': humidity_data
    }

    # One temperature and one humidity column per sensor
    for name in sensor_names:
        data[f'{name}_temperature (°C)'] = sensor_temperature_data[name]
        data[f'{name}_humidity (RH%)'] = sensor_humidity_data[name]

    df = pd.DataFrame(data)

    # File name starts with the date and time at which the acquisition stopped
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f'{timestamp}_{FILE_LABEL}.csv'
    df.to_csv(filename, index=False)
    print(f"Data saved to {filename}")

finally:
    # Always release the serial port, even if an error occurred
    ser.close()
