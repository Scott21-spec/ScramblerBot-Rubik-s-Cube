import tkinter as tk
from tkinter import filedialog

def abrir_selector_archivos():
    root = tk.Tk()
    root.withdraw()  # Oculta la ventana principal
    root.attributes('-topmost', True)  # Fuerza la ventana al frente
    file_path = filedialog.askopenfilename(
        title="Seleccionar secuencia WCA",
        filetypes=[("Archivos de texto", "*.txt")]
    )
    if file_path:
        with open(file_path, 'r') as file:
            return file.read().strip()
    return None