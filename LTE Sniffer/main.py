#!/usr/bin/env python
import kivy
import mysql.connector
import subprocess
import sys
import os
from kivy.config import Config
Config.set('kivy', 'desktop', 1)
Config.write()
Config.set('kivy', 'exit_on_escape', 1)
Config.write()
Config.set('graphics', 'width', '850')
Config.set('graphics', 'height', '840')
from kivy.uix.gridlayout import GridLayout
from kivy.app import App
from kivy.uix.button import Button

from kivy.uix.widget import Widget
from kivy.core.window import Window
    
class LTE_Sniffer(App):
            
    def build(self):
        self.db = mysql.connector.connect(host="localhost",user="root",passwd="ilovegeniussnorlax",db='LTESnifferDB')
        self.cur = self.db.cursor()
        self.cur.execute("SELECT * FROM LTESnifferTable;")
        self._keyboard = Window.request_keyboard(self._keyboard_closed, self)
        self._keyboard.bind(on_key_down=self._on_keyboard_down)
        self.layout = GridLayout(cols=7, row_force_default=True, row_default_height=40)
        self.layout.add_widget(Button(text='Frequency\MHz'))
        self.layout.add_widget(Button(text='MCC'))
        self.layout.add_widget(Button(text='MNC'))
        self.layout.add_widget(Button(text='Carrier'))
        self.layout.add_widget(Button(text='Physical Cell ID'))
        self.layout.add_widget(Button(text='PSS Power\dBm'))
        self.layout.add_widget(Button(text='Last Seen Day'))
        self.update()
        return self.layout

    def update(self):
        for row in self.cur.fetchall():
            for col in range (0,7):
                self.layout.add_widget(Button(text=str(row[col])))

    def _keyboard_closed(self):
        self._keyboard.unbind(on_key_down=self._on_keyboard_down)
        self._keyboard = None

    def _on_keyboard_down(self, keyboard, keycode, text, modifiers):
        if keycode[1] == 'u':
            os.execl(sys.executable, sys.executable, *sys.argv)
##            subprocess.call['./FileToCSv.sh']
        return True
    
if __name__ == '__main__':
    LTE_Sniffer().run()
