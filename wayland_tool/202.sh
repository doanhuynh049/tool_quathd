echo "Test Case 202: Resume from inactive mode"
sed -i 's/--idle-time=0/--idle-time=30/' /lib/systemd/system/weston.service
echo "2. Restart the system with the new configuration."
reboot

