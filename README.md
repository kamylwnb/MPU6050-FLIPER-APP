MPU-6050 Sensor Reader for Flipper Zero 🚀
A dedicated Flipper Zero application that allows users to easily interface with and read data from an external MPU-6050 Accelerometer and Gyroscope module via the I2C bus. Perfect for basic motion sensing, vibration analysis, or educational purposes.

✨ Key Features for Users
This application transforms your Flipper Zero into a real-time 3-axis G-force meter:

📈 Real-Time Data Display
Live Acceleration (G-force): See the instantaneous acceleration values along the X, Y, and Z axes displayed clearly on the main screen, measured in Gs (g).

Sensor Status: The application checks for sensor connection and displays a clear message if the MPU-6050 is not detected or initialized, preventing confusion.

📊 Maximum G-Force Tracking
Peak Measurement: Track and display the maximum G-force recorded separately for the X, Y, and Z axes since the last reset. This is ideal for measuring sudden impacts or peak accelerations.

Easy Reset: A simple key press allows you to instantly reset the maximum recorded values to zero, starting a fresh measurement cycle.

⚙️ Customizable Sensor Settings
Access a Settings menu to fine-tune the sensor's behavior directly from the Flipper Zero:

I2C Address Selection: Quickly switch between the two common MPU-6050 I2C addresses (0x68 or 0x69) to accommodate different module wiring (controlled by the AD0 pin).

Accelerometer Full-Scale Range (FSR): Adjust the sensitivity and maximum range of the accelerometer to fit your use case, with options for:

±2g

±4g (Default)

±8g

±16g

Gyroscope Full-Scale Range (FSR): Configure the measurement range for the gyroscope (although currently only acceleration data is displayed), with options up to ±2000 degrees per second.

🧭 Intuitive Navigation
The application features a clean, simple menu structure:

Main Screen: Displays current acceleration and provides quick access to Settings, About, and Max G sub-menus.

Settings Screen: Allows cursor-based navigation (↑/↓) and value changes (←/→) for all configuration options.

About Screen: Provides basic application information.

📌 How to Use
Connect: Wire your MPU-6050 module to your Flipper Zero's GPIO pins, ensuring correct I2C connections (SDA, SCL).

Run: Launch the application on your Flipper Zero.

Monitor: The main screen immediately starts displaying real-time acceleration data.

Configure: Navigate to Settings (Left button) to change I2C address or sensitivity (FSR).

View Peaks: Navigate to Max G Values (OK button) to see the highest recorded acceleration.
