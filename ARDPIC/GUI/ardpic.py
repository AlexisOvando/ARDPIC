import tkinter as tk
from tkinter import filedialog, messagebox
from tkinter import ttk
import serial
import serial.tools.list_ports
import threading

class PICProgrammerUI:
    def __init__(self, root):
        self.root = root
        self.root.title("ARDPIC")
        self.root.geometry("530x400")  # Tamaño de la ventana
        self.root.config(bg="#f4f4f9")  # Fondo color gris claro
        #self.root.iconbitmap("logo.ico")  # Ruta del archivo .ico
        self.port = tk.StringVar()
        self.filename = tk.StringVar()
        self.serial_connection = None
        self.hex_data = {}

        # Font and style configuration
        self.font = ('arial', 12)
        self.button_font = ('arial', 10, 'bold')
        
        # Frame for inputs and buttons
        input_frame = tk.Frame(root, bg="#f4f4f9")
        input_frame.grid(row=0, column=0, padx=20, pady=20, sticky="w")

        # Port entry with Combobox
        tk.Label(input_frame, text="Port:", font=self.font, bg="#f4f4f9").grid(row=0, column=0, padx=10, pady=5, sticky="e")
        self.port_combobox = ttk.Combobox(input_frame, textvariable=self.port, font=self.font, state="normal")
        self.port_combobox.grid(row=0, column=1, padx=10, pady=5)
        self.refresh_ports()  # Populate the combobox with available COM ports
        
        # File selection
        tk.Label(input_frame, text="Hex File:", font=self.font, bg="#f4f4f9").grid(row=1, column=0, padx=10, pady=5, sticky="e")
        tk.Entry(input_frame, textvariable=self.filename, font=self.font).grid(row=1, column=1, padx=10, pady=5)
        tk.Button(input_frame, text="Browse", command=self.browse_file, font=self.button_font, bg="#4CAF50", fg="white", relief="raised").grid(row=1, column=2, padx=10, pady=5)

        # Action buttons
        action_frame = tk.Frame(root, bg="#f4f4f9")
        action_frame.grid(row=2, column=0, padx=90, pady=10, sticky="w")
        
        tk.Button(action_frame, text="Program", command=lambda: self.run_thread(self.program), font=self.button_font, bg="#008CBA", fg="white", relief="raised").grid(row=0, column=0, padx=10, pady=10)
        tk.Button(action_frame, text="Verify", command=lambda: self.run_thread(self.verify), font=self.button_font, bg="#f44336", fg="white", relief="raised").grid(row=0, column=1, padx=10, pady=10)
        tk.Button(action_frame, text="Debug", command=lambda: self.run_thread(self.debug), font=self.button_font, bg="#ff9800", fg="white", relief="raised").grid(row=0, column=2, padx=10, pady=10)
        tk.Button(action_frame, text="Clear Console", command=self.clear_console, font=self.button_font, bg="#9E9E9E", fg="white", relief="raised").grid(row=0, column=3, padx=10, pady=10)

        # Console output
        tk.Label(root, text="Console Output:", font=self.font, bg="#f4f4f9").grid(row=3, column=0, columnspan=4, padx=20, pady=5)
        self.console = tk.Text(root, height=10, width=60, font=('Courier', 10), bg="#333", fg="white", wrap="word")
        self.console.grid(row=4, column=0, columnspan=4, padx=20, pady=5)

        # Update the list of available COM ports every 2 seconds
        self.update_ports_list()

    def browse_file(self):
        filename = filedialog.askopenfilename(filetypes=[("Hex files", "*.hex")])
        self.filename.set(filename)

    def log(self, message):
        self.console.insert(tk.END, message + "\n")
        self.console.see(tk.END)

    def clear_console(self):
        self.console.delete('1.0', tk.END)

    def refresh_ports(self):
        """ Get available COM ports and update the combobox. """
        ports = [port.device for port in serial.tools.list_ports.comports()]
        if ports:
            self.port_combobox['values'] = ports
            self.port_combobox.current(0)  # Set the first available port as default

    def update_ports_list(self):
        """ Update the available COM ports every 2 seconds. """
        self.refresh_ports()  # Refresh the list of COM ports
        self.root.after(2000, self.update_ports_list)  # Call this function again after 2 seconds

    def connect_serial(self):
        """ Establish serial connection. """
        try:
            self.serial_connection = serial.Serial(self.port.get().strip(), 9600, timeout=1)
            self.log(f"Connected to port: {self.port.get()}")
        except Exception as e:
            self.log(f"Error: Unable to open port. {e}")
            self.serial_connection = None

    def close_serial(self):
        """ Close serial connection. """
        if self.serial_connection and self.serial_connection.is_open:
            self.serial_connection.close()
            self.log("Serial connection closed.")

    def load_hex_file(self):
        """ Load hex file data. """
        try:
            with open(self.filename.get(), "r") as hex_file:
                self.hex_data.clear()
                for line in hex_file:
                    if int(line[3:7], 16) // 2 != 0x2007 and len(line) != 11:
                        address = int(line[3:7], 16) // 2
                        n = int(line[1:3], 16) // 2
                        for i in range(n):
                            self.hex_data[address] = (
                                bytes.fromhex(line[9 + 4 * i:11 + 4 * i]),
                                bytes.fromhex(line[11 + 4 * i:13 + 4 * i]),
                            )
                            address += 1
                self.log(f"Hex file loaded successfully. {len(self.hex_data)} words read.")
        except Exception as e:
            self.log(f"Error: Could not read hex file. {e}")
            self.hex_data.clear()

    def validate_inputs(self):
        """ Validate if the user has filled all required inputs. """
        if not self.port.get():
            self.log("Error: Port not specified.")
            return False
        if not self.filename.get():
            self.log("Error: Hex file not specified.")
            return False
        if not self.serial_connection or not self.serial_connection.is_open:
            self.connect_serial()
            if not self.serial_connection:
                return False
        if not self.hex_data:
            self.load_hex_file()
            if not self.hex_data:
                return False
        return True

    def program(self):
        """ Program PIC using the hex file. """
        if not self.validate_inputs():
            return

        try:
            self.log("Programming started...")
            for address, word in self.hex_data.items():
                self.serial_connection.write(b'\x01')  # Request to send a word
                if self.serial_connection.read() == b'\x01':  # Confirmation
                    self.serial_connection.write(word[0])
                    self.serial_connection.write(word[1])
            self.log("Programming completed successfully.")
        except Exception as e:
            self.log(f"Error during programming: {e}")

    def verify(self):
        """ Verify PIC memory contents with hex file. """
        if not self.validate_inputs():
            return

        try:
            self.log("Verification started...")
            for address, word in self.hex_data.items():
                self.serial_connection.write(b'\x01')  # Request to send a word
                if self.serial_connection.read() == b'\x01':  # Confirmation
                    self.serial_connection.write(word[0])
                    self.serial_connection.write(word[1])
            self.log("Verification completed successfully.")
        except Exception as e:
            self.log(f"Error during verification: {e}")

    def debug(self):
        """ Perform debugging on PIC. """
        if not self.validate_inputs():
            return

        try:
            self.log("Debugging started...")
            self.serial_connection.write(b'\x04')  # Request the number of words
            response = self.serial_connection.read(2)
            if response:
                total_words = int.from_bytes(response, byteorder='little')
                self.log(f"Debug: Total words in memory: {total_words}")
            else:
                self.log("No response from device during debug.")
        except Exception as e:
            self.log(f"Error during debugging: {e}")

    def run_thread(self, target_function):
        """ Run a target function in a separate thread. """
        threading.Thread(target=target_function, daemon=True).start()

def main():
    root = tk.Tk()
    app = PICProgrammerUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()
