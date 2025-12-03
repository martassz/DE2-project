
from PySide6.QtWidgets import QMainWindow, QWidget, QHBoxLayout, QVBoxLayout, QLabel, QFileDialog, QMessageBox
from PySide6.QtCore import Slot
from ui.panels.sidebar import Sidebar
from ui.realtime_plot import RealtimePlotWidget

# Simple parser for the TXT format specified by the user.
import datetime as dt

def parse_txt_file(path):
    """
    Parse lines like:
    00:20:05, 24.37, 981.88, 47.44, 100

    Returns:
      times: list of POSIX epoch seconds (float)
      data: dict with keys 'temperature','pressure','humidity','light' each mapping to list of floats
    """
    times = []
    temps = []
    pressures = []
    hums = []
    lights = []

    with open(path, "r", encoding="utf-8") as fh:
        lines = fh.readlines()

    base_date = dt.date.today()
    prev_dt = None

    for ln in lines:
        ln = ln.strip()
        if not ln:
            continue
        if ln.startswith("#"):
            continue

        parts = [p.strip() for p in ln.split(",")]
        if len(parts) < 5:
            parts = ln.split()
            if len(parts) < 5:
                continue

        time_str = parts[0]
        try:
            t = dt.datetime.strptime(time_str, "%H:%M:%S").time()
        except Exception:
            # skip lines that don't start with time in expected format
            continue

        # assemble full datetime, handling day rollover
        curr_dt = dt.datetime.combine(base_date, t)
        if prev_dt is not None and curr_dt < prev_dt:
            # rolled over past midnight -> advance base_date by one day
            base_date = base_date + dt.timedelta(days=1)
            curr_dt = dt.datetime.combine(base_date, t)

        prev_dt = curr_dt
        times.append(curr_dt.timestamp())

        # parse channels (be forgiving)
        def to_float(s):
            try:
                return float(s)
            except Exception:
                return float('nan')

        temps.append(to_float(parts[1]))
        pressures.append(to_float(parts[2]))
        hums.append(to_float(parts[3]))
        lights.append(to_float(parts[4]))

    data = {
        'temperature': temps,
        'pressure': pressures,
        'humidity': hums,
        'light': lights
    }

    return times, data



class MainWindow(QMainWindow):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("ESP -> TXT plotter")
        self.resize(1000, 600)

        central = QWidget(self)
        self.setCentralWidget(central)
        layout = QHBoxLayout(central)

        # Sidebar (left)
        self.sidebar = Sidebar(self)
        layout.addWidget(self.sidebar)

        # Plot area (right)
        self.plot = RealtimePlotWidget(time_window_s=60.0, parent=self)
        layout.addWidget(self.plot, 1)

        # Connections
        self.sidebar.open_txt_clicked.connect(self.open_txt)
        self.sidebar.selection_changed.connect(self.on_selection_changed)

        # data storage
        self._times = []
        self._data = {}

    @Slot()
    def open_txt(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select TXT file", "", "Text files (*.txt);;All files (*)")
        if not path:
            return
        try:
            times_s, data = parse_txt_file(path)
        except Exception as e:
            QMessageBox.critical(self, "Parsing error", f"Failed to parse file:\\n{e}")
            return

        if not times_s or not data:
            QMessageBox.information(self, "No data", "No valid data found in the file.")
            return

        self._times = times_s
        print("SAMPLED TIMES (first 5):", self._times[:5])
        self._data = data
        # Update plot with currently selected channels
        self.plot.set_data(self._times, self._data, self.sidebar.get_selected_channels())

    @Slot()
    def on_selection_changed(self):
        # Redraw using previously loaded data
        if not self._times or not self._data:
            return
        self.plot.set_data(self._times, self._data, self.sidebar.get_selected_channels())
