#!/bin/bash

# Start the 3D application and let it stand for 1 minute
./OGLES3Navigation3D -fps &
APP_PID=$!
echo "3D application started with PID $APP_PID. Letting it stand for 1 minute..."
sleep 10
echo "10s"
sleep 20
echo "20s"
sleep 30
echo "30s"

# Check if the 3D application is running correctly
if ! ps -p $APP_PID > /dev/null
then
    echo "3D application did not start correctly."
    exit 1
else
    echo "3D Application is running correctly. (#1)"
fi
killall OGLES3Navigation3D
# Start the texture pixmap application
./OES3_Texture_Pixmap_EGLImage_wl &
TEXTURE_APP_PID=$!
echo "Texture Pixmap application started with PID $TEXTURE_APP_PID."
# No explicit sleep is required here as per the test case, but it's generally a good idea to wait for a bit
sleep 5

# Check if the texture pixmap application continues to run
if ! ps -p $TEXTURE_APP_PID > /dev/null
then
    echo "Texture Pixmap application did not continue running."
    exit 1
else
    echo "3D Application continues to run. (#2)"
fi

# Terminate the texture pixmap application
kill -2 `pidof OES3_Texture_Pixmap_EGLImage_wl`
echo "Sent SIGINT to terminate the Texture Pixmap application."

# Wait a bit and then check if the application has been terminated
sleep 5
if ps -p $TEXTURE_APP_PID > /dev/null
then
    echo "Texture Pixmap application did not terminate correctly."
    exit 1
else
    echo "3D Application is terminated correctly. (#3)"
fi

# End of script
echo "Test completed successfully."

