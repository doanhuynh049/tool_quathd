echo "Test Case 201: Boot up weston with no input device"
echo -e "[core]\nrequire-input=false" >> /etc/xdg/weston/weston.ini
echo "2. Disconnect all input devices."
sleep 10
echo "3. Make sure that weston is started and idle screen is displayed. (#1)"
sleep 5
reboot

