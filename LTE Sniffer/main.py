import kivy
from kivy.config import Config
Config.set('graphics', 'width', '850')
Config.set('graphics', 'height', '840')
from kivy.uix.gridlayout import GridLayout
from kivy.app import App
from kivy.uix.button import Button

from kivy.uix.widget import Widget
from kivy.core.window import Window
    
class LTE_Sniffer(App):
            
    def build(self):
        self._keyboard = Window.request_keyboard(self._keyboard_closed, self)
        self._keyboard.bind(on_key_down=self._on_keyboard_down)
        self.layout = GridLayout(cols=7, row_force_default=True, row_default_height=40)
        self.layout.add_widget(Button(text='Frequency'))
        self.layout.add_widget(Button(text='MCC'))
        self.layout.add_widget(Button(text='MNC'))
        self.layout.add_widget(Button(text='Carrier'))
        self.layout.add_widget(Button(text='Physical Cell ID'))
        self.layout.add_widget(Button(text='PSS Power'))
        self.layout.add_widget(Button(text='Last Seen Due'))
        self.update()
        return self.layout

    def update(self):
        for x in range (0,140):
            self.layout.add_widget(Button(text=str(x)))

    def _keyboard_closed(self):
        self._keyboard.unbind(on_key_down=self._on_keyboard_down)
        self._keyboard = None

    def _on_keyboard_down(self, keyboard, keycode, text, modifiers):
        if keycode[1] == 'u':
            print("test test")
        return True
    
if __name__ == '__main__':
    LTE_Sniffer().run()

##class Grid(Widget):
##    def prepare_grid(self):
##        self.layout = GridLayout(cols=7, row_force_default=True, row_default_height=40)
##        self.layout.add_widget(Button(text='MCC'))
##        self.layout.add_widget(Button(text='MNC'))
##        self.layout.add_widget(Button(text='Carrier'))
##        self.layout.add_widget(Button(text='Frequency'))
##        self.layout.add_widget(Button(text='Physical Cell ID'))
##        self.layout.add_widget(Button(text='PSS Power'))
##        self.layout.add_widget(Button(text='Last Seen Due'))
##        self.update()
##        return self.layout
##
##    def update(self):
##      for x in range (0,140):
##            self.layout.add_widget(Button(text='test'))
##        
##            
##class LTE_Sniffer(App):
##
##    def build(self):
##        table = Grid()
##        table.prepare_grid()
##        return table
##
##if __name__ == '__main__':
##    LTE_Sniffer().run()
        
        
