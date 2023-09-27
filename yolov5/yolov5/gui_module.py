from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtGui import QImage, QPixmap
from PyQt5.QtWidgets import QApplication, QMainWindow, QLabel, QSlider, QVBoxLayout, QWidget, QPushButton, QHBoxLayout, QButtonGroup
from utils.general import cv2



class CustomWidget(QWidget):
    def __init__(self, parent=None):
        super(CustomWidget, self).__init__(parent)

    def keyPressEvent(self, event):
        if event.key() == Qt.Key_Escape:
            self.close()
            sys.exit()


def set_zoom(value):
	if 0 <= value <=260:
		cmd = ["v4l2-ctl", "--set-ctrl=zoom_absolute={}".format(value)]
		subprocess.run(cmd)


def button1_func():
    global select
    select = 'drone'

def button2_func():
    global select
    select = 'jet'

def button3_func():
    global select
    select = 'bird'

def button4_func():
    global mode
    mode = 1

def button5_func():
    global mode
    mode = 0



def gui_setting():
    app = QApplication([])

    main_widget = CustomWidget()
    main_widget.resize(1300, 740)
    main_widget.setWindowTitle("Turret System")

    main_layout = QHBoxLayout(main_widget)
    v_layout = QVBoxLayout()

    image_label = QLabel(main_widget)

    v_layout.addWidget(image_label)


    slider = QSlider(Qt.Horizontal, main_widget)
    slider.setMinimum(0)
    slider.setMaximum(260)
    slider.valueChanged.connect(set_zoom)
    v_layout.addWidget(slider)

      
    button1 = QPushButton("Drone", main_widget)
    button1.setCheckable(True)
    button1.setChecked(True)
    button1.setFixedSize(150, 60)
    button1.toggled.connect(button1_func)

    button2 = QPushButton("Jet", main_widget)
    button2.setCheckable(True)
    button2.setFixedSize(150, 60)
    button2.toggled.connect(button2_func)

    button3 = QPushButton("Bird", main_widget)
    button3.setCheckable(True)
    button3.setFixedSize(150, 60)
    button3.toggled.connect(button3_func)


    button4 = QPushButton("Auto - aim", main_widget)
    button4.setCheckable(True)
    button4.setFixedSize(150, 60)
    button4.toggled.connect(button4_func)

    button5 = QPushButton("Manual", main_widget)
    button5.setCheckable(True)
    button5.setChecked(True)
    button5.setFixedSize(150, 60)
    button5.toggled.connect(button5_func)

    button_group = QButtonGroup(main_widget)
    button_group.addButton(button1)
    button_group.addButton(button2)
    button_group.addButton(button3)

    button_group2 = QButtonGroup(main_widget)
    button_group2.addButton(button4)
    button_group2.addButton(button5)
    
    vbox_all_buttons = QVBoxLayout()
    vbox_all_buttons.addWidget(button1)
    vbox_all_buttons.addWidget(button2)
    vbox_all_buttons.addWidget(button3)
    vbox_all_buttons.addStretch(2)
    vbox_all_buttons.addWidget(button4)
    vbox_all_buttons.addWidget(button5)
  
    main_layout.addLayout(v_layout)
    main_layout.addLayout(vbox_all_buttons)

    main_widget.setLayout(main_layout)
    main_widget.show()

def gui_window(frame):
    frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    height, width, channel = frame.shape
    bytes_per_line = 3 * width
    q_image = QImage(frame.data, width, height, bytes_per_line, QImage.Format_RGB888)
    pixmap = QPixmap.fromImage(q_image)
            
    image_label.setPixmap(pixmap.scaled(1280, 720, Qt.KeepAspectRatio))

    app.processEvents()

def gui_exec():
    app.exec_()


