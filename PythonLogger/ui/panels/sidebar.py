from PySide6.QtWidgets import QFrame, QVBoxLayout, QLabel, QPushButton, QCheckBox, QGroupBox
from PySide6.QtCore import Signal, Qt

class Sidebar(QFrame):
    """
    Left sidebar containing:
      - 'Connect TXT' button (open a TXT file)
      - 'Select data' group of checkboxes for Temperature, Pressure, Humidity, Light
    Signals:
      - open_txt_clicked: emitted when user requests to open a TXT file
      - selection_changed: emitted whenever checkboxes change
    """

    open_txt_clicked = Signal()
    selection_changed = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName('sidebar')
        self.setFixedWidth(260)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10,10,10,10)

        title = QLabel("Select data")
        title.setAlignment(Qt.AlignLeft)
        layout.addWidget(title)

        # Button to open TXT file
        self.btn_open = QPushButton("Connect TXT")
        self.btn_open.clicked.connect(self._on_open_clicked)
        layout.addWidget(self.btn_open)

        # Checkboxes (exclusive selection)
        box = QGroupBox("Environmental Data")
        box_layout = QVBoxLayout(box)

        self.cb_temp = QCheckBox("Temperature [°C]")
        self.cb_pressure = QCheckBox("Pressure")
        self.cb_humidity = QCheckBox("Humidity")
        self.cb_light = QCheckBox("Light")

        # Default selected option
        self.cb_temp.setChecked(True)

        # Connect exclusive selection logic
        self.cb_temp.stateChanged.connect(self._exclusive_select)
        self.cb_pressure.stateChanged.connect(self._exclusive_select)
        self.cb_humidity.stateChanged.connect(self._exclusive_select)
        self.cb_light.stateChanged.connect(self._exclusive_select)

        # Also connect to external selection_changed signal
        for cb in [self.cb_temp, self.cb_pressure, self.cb_humidity, self.cb_light]:
            cb.stateChanged.connect(self.selection_changed)
            box_layout.addWidget(cb)

        layout.addWidget(box)
        layout.addStretch()

    def _on_open_clicked(self):
        self.open_txt_clicked.emit()

    def _exclusive_select(self, state):
        """Allow only one checkbox to be selected at a time."""
        if state == 0:
            return  # ignore uncheck events (avoid loops)

        sender = self.sender()

        for cb in [self.cb_temp, self.cb_pressure, self.cb_humidity, self.cb_light]:
            if cb is not sender:
                cb.blockSignals(True)
                cb.setChecked(False)
                cb.blockSignals(False)

    def get_selected_channels(self):
        """Returns the selected channel keyword."""
        if self.cb_temp.isChecked():
            return ['temperature']
        if self.cb_pressure.isChecked():
            return ['pressure']
        if self.cb_humidity.isChecked():
            return ['humidity']
        if self.cb_light.isChecked():
            return ['light']
        return []
