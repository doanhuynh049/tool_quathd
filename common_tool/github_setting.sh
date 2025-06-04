#!/bin/bash

# Update packages and install git
echo "Updating packages and installing Git..."
sudo apt update
sudo apt install git -y

# Git configuration
echo "Configuring Git..."
read -p "Enter your Git username: " gitusername
read -p "Enter your Git email: " gitemail

git config --global user.name "$gitusername"
git config --global user.email "$gitemail"

# Generate SSH key
echo "Generating SSH key..."
ssh-keygen -t rsa -b 4096 -C "$gitemail"

# Start the SSH agent in the background
eval "$(ssh-agent -s)"

# Add SSH key to the ssh-agent
ssh-add ~/.ssh/id_rsa

# Copy SSH key to clipboard for GitHub
echo "Copying SSH key to clipboard. Please add this SSH key to your GitHub account."
xclip -sel clip < ~/.ssh/id_rsa.pub

# Test instructions
echo "Once you have added this SSH key to your GitHub account, run the following command to test:"
echo "ssh -T git@github.com"

echo "Setup complete."

