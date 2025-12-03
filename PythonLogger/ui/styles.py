# --- MODERN DARK THEME ---
STYLESHEET = """
QMainWindow {
    background-color: #1e1e1e;
    color: #e0e0e0;
}
QWidget {
    font-family: 'Segoe UI', sans-serif;
    font-size: 14px;
    color: #e0e0e0;
}

/* --- SIDEBAR --- */
QFrame#Sidebar {
    background-color: #252526;
    border-right: 1px solid #3e3e42;
}

/* --- VALUE CARDS --- */
QFrame#ValueCard {
    background-color: #2d2d30; 
    border-radius: 6px;
    border: none;
}
QFrame#ValueCard:hover {
    background-color: #383838;
    border: 1px solid #404040;
}
QLabel#ValueTitle {
    color: #007acc; /* Modrý nadpis */
    font-size: 13px;
    font-weight: bold;
    text-transform: uppercase;
}
QLabel#ValueNumber {
    color: #ffffff;
    font-size: 26px;
    font-weight: bold;
}

/* --- TLAČÍTKA --- */
QPushButton {
    background-color: #3e3e42;
    border: none;
    color: white;
    padding: 10px;
    border-radius: 5px;
    font-weight: bold;
}
QPushButton:hover {
    background-color: #505050;
}
QPushButton:disabled {
    background-color: #2d2d30;
    color: #606060;
    border: 1px solid #333333;
}

QPushButton#BtnConnect { background-color: #007acc; }
QPushButton#BtnConnect:hover { background-color: #0098ff; }
QPushButton#BtnConnect:disabled { background-color: #2d2d30; color: #aaaaaa; }

QPushButton#BtnStart { background-color: #2ea043; }
QPushButton#BtnStart:hover { background-color: #3fb950; }
QPushButton#BtnStart:disabled { background-color: #2d2d30; color: #606060; }

QPushButton#BtnStop { background-color: #da3633; }
QPushButton#BtnStop:hover { background-color: #f85149; }
QPushButton#BtnStop:disabled { background-color: #2d2d30; color: #606060; }

/* --- COMBOBOX --- */
QComboBox {
    background-color: #333337;
    border: 1px solid #505050;
    padding: 5px;
    border-radius: 4px;
    color: white;
}
QComboBox:hover {
    border: 1px solid #007acc;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}

QComboBox QAbstractItemView {
    background-color: #252526;
    color: white;
    border: 1px solid #3e3e42;
    selection-background-color: #007acc;
    selection-color: white;
    outline: 0;
}

/* --- MODAL WINDOWS --- */
QMessageBox { background-color: #252526; color: #e0e0e0; }
QMessageBox QLabel { color: #e0e0e0; }
QMessageBox QPushButton {
    width: 80px;
    background-color: #3e3e42;
    border: 1px solid #505050;
}
QMessageBox QPushButton:hover { background-color: #505050; }

/* --- PROGRESS BAR --- */
QProgressBar {
    border: 1px solid #3e3e42;
    border-radius: 4px;
    text-align: center;
    background-color: #252526;
}
QProgressBar::chunk {
    background-color: #007acc;
    border-radius: 3px;
}

/* --- SCROLLBAR --- */
QScrollArea {
    border: none;
    background-color: transparent;
}
QScrollBar:horizontal {
    border: none;
    background: #252526;
    height: 8px;
    margin: 0px;
}
QScrollBar::handle:horizontal {
    background: #424242;
    min-width: 20px;
    border-radius: 4px;
}
QScrollBar::handle:horizontal:hover {
    background: #606060;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    background: none;
    border: none;
}
"""