# ui/realtime_plot.py (cleaned)
from PySide6.QtWidgets import QWidget, QVBoxLayout
import pyqtgraph as pg
import datetime as dt

CHANNEL_META = {
    "temperature": ("Temperature", "°C"),
    "pressure": ("Pressure", "hPa"),
    "humidity": ("Humidity", "%"),
    "light": ("Light", "%")
}

class TimeAxisItem(pg.AxisItem):
    """AxisItem which expects x-values as POSIX timestamps (seconds since epoch).

    It will show labels formatted as HH:MM:SS (local time).
    """
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def tickStrings(self, values, scale, spacing):
        strs = []
        for v in values:
            try:
                ts = float(v)
                dtobj = dt.datetime.fromtimestamp(ts)
                strs.append(dtobj.strftime("%H:%M:%S"))
            except Exception:
                strs.append("")
        return strs


class RealtimePlotWidget(QWidget):
    def __init__(self, time_window_s: float = 60.0, parent=None):
        super().__init__(parent)
        self._time_window_s = time_window_s

        layout = QVBoxLayout(self)
        # use TimeAxisItem as bottom axis so tickStrings gets epoch seconds
        self._plot_widget = pg.PlotWidget(axisItems={'bottom': TimeAxisItem(orientation='bottom')})
        self._plot_widget.showGrid(x=True, y=True, alpha=0.3)
        self._plot_widget.addLegend()

        layout.addWidget(self._plot_widget)

        self._curves = {}  # key -> plot curve

    def clear(self):
        self._plot_widget.clear()
        self._plot_widget.addLegend()
        self._curves = {}

    def set_data(self, times_epoch_s, data_dict, selected_keys):
        """
        times_epoch_s: list of floats (POSIX epoch seconds)
        data_dict: dict of key -> list of floats (same length as times)
        selected_keys: list of keys to plot (subset of data_dict keys)
        """
        # Debugging: uncomment to inspect inputs
        # print("DEBUG times (first 5):", times_epoch_s[:5])

        # Remove curves not selected
        for k in list(self._curves.keys()):
            if k not in selected_keys:
                item = self._curves.pop(k)
                try:
                    self._plot_widget.removeItem(item)
                except Exception:
                    pass

        # Add or update selected curves
        for idx, key in enumerate(selected_keys):
            if key not in data_dict:
                continue
            y = data_dict[key]
            # Ensure lengths match — truncate if needed
            n = min(len(times_epoch_s), len(y))
            x = times_epoch_s[:n]
            y = y[:n]

            if key in self._curves:
                self._curves[key].setData(x=x, y=y)
            else:
                pen = pg.mkPen(self._color_for_key(key), width=2)
                curve = self._plot_widget.plot(x=x, y=y, pen=pen,
                                               name=f"{CHANNEL_META.get(key,(key,''))[0]} ({CHANNEL_META.get(key,(key,''))[1]})",
                                               antialias=True)
                self._curves[key] = curve

        # autoscale x to full data and add small margins
        if times_epoch_s:
            try:
                xmin = min(times_epoch_s)
                xmax = max(times_epoch_s)
                self._plot_widget.setXRange(xmin, xmax, padding=0.02)
            except Exception:
                pass

    def _color_for_key(self, key):
        fixed_colors = {
            "temperature": "#FF0000",  # red
            "pressure":    "#00FF00",  # green
            "humidity":    "#0000FF",  # blue
            "light":       "#FFA500",  # orange
        }
        return fixed_colors.get(key, "#FFFFFF")  # default white
