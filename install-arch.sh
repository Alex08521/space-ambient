#!/bin/bash
# Install script for Arch Linux
# Space Ambient - Background ambient music daemon for KDE Plasma 6

set -e

echo "=== Space Ambient Installer for Arch Linux ==="

# Check if running as root (optional, can install locally without root)
if [ "$EUID" -ne 0 ]; then
    echo "Note: Running as non-root user. Installation will require sudo privileges."
fi

# Update package database and install dependencies
echo "Installing dependencies..."
sudo pacman -Syu --noconfirm \
    cmake \
    make \
    gcc \
    extra-cmake-modules \
    qt6-base \
    qt6-tools \
    kwindowsystem \
    libpulse \
    libvorbis \
    systemd

# Create build directory
echo "Creating build directory..."
rm -rf build
mkdir build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

# Build
echo "Building..."
make

# Install
echo "Installing..."
sudo make install

# Reload systemd user daemon
echo "Reloading systemd user daemon..."
systemctl --user daemon-reload

# Enable and start the service
echo "Enabling and starting space-ambient service..."
systemctl --user enable space-ambient.service
systemctl --user start space-ambient.service

echo ""
echo "=== Installation Complete ==="
echo "Space Ambient has been installed and started."
echo ""
echo "To check the service status:"
echo "  systemctl --user status space-ambient.service"
echo ""
echo "To stop the service:"
echo "  systemctl --user stop space-ambient.service"
echo ""
echo "Enjoy using Space Ambient!"
