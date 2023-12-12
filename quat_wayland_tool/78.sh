echo "Test Case 78: 3D Application (using Pixmap)"
echo "1. Let stand for 1 minute or more after starting the 3D Application."
./OGLES3Navigation3D -fps &
echo "2. Run the following command from weston-terminal."
./OES3_Texture_Pixmap_EGLImage_wl &
sleep 60 && kill -2 `pidof OES3_Texture_Pixmap_EGLImage_wl`
