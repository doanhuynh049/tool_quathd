sudo fdisk -l  		# Identify the SD card device, replace /dev/sdx with your device
sudo umount /dev/sdx2  	# Unmount the partition, replace /dev/sdx2 with the actual partition
sudo mkfs.ext4 /dev/sdx2  # Format the partition to EXT4
mkdir /mnt/sd  # Create a mount point
sudo mount /dev/sdx2 /mnt/sd  # Mount the partition
sudo cp -r /path/to/filesystem/* /mnt/sd/  # Copy the filesystem to the SD card
sudo umount /mnt/sd  # Unmount the partition
