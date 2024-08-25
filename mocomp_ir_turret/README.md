# Motion compensated IR Turret
This mod adds an Adafruit LIS3DH 3DOF accelerometer to the IR turret to compensate pitch motion. My plan is to attach the turret base to a hat and always keep the turret pointing at the same vertical position.

At some point I may switch to a 6DOF IMU with gyros so I can compensate the Yaw too.

I placed the Adafruit sensor on the back plate that holds the roll motor, with the sensor just above the wire bundle that passes through. Looking at the back of the turret, the board is mounted so that the X arrow points to the left, the Y arrow points down, and the Z comes out of the board towards you.

I'm using the default I2C address, and it is connected to the nano's A4/A5 pins for I2C control.