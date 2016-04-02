import kivy
from kivy.uix.gridlayout import GridLayout
from kivy.app import App
from kivy.uix.button import Button

    
class LTE_Sniffer(App):
    
    def update(self):
        for x in range (0,140):
            layout.add_widget(Button(text='test'))
            return layout
            
    def build(self):
        layout = GridLayout(cols=7, row_force_default=True, row_default_height=40)
        layout.add_widget(Button(text='MCC'))
        layout.add_widget(Button(text='MNC'))
        layout.add_widget(Button(text='Carrier'))
        layout.add_widget(Button(text='Frequency'))
        layout.add_widget(Button(text='Physical Cell ID'))
        layout.add_widget(Button(text='PSS Power'))
        layout.add_widget(Button(text='Last Seen Due'))
        LTE_Sniffer.update()
        return layout
    
if __name__ == '__main__':
    LTE_Sniffer().run()
        
