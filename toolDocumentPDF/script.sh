#!/bin/bash
numberoffile=18
# Function to copy file to the corresponding folder
copy_file_to_folder() {
    local file=$1
    local prefix="RENESAS_"
    local suffix="_UME_v3.1.0.pdf"

    # Extract the key part of the file name
    local key_part=${file#${prefix}}
    key_part=${key_part%%${suffix}}

    # Map the key part to folder names
    case "$key_part" in
        RCH3M3M3NE3D3V3H_ThermalDriver)
            cp "$file" thermal/
            ;;
        RCH3M3M3NE3D3V3UV3HV3M_CMEM)
            cp "$file" cmem/
            ;;
        RCH3M3M3NE3D3V3UV3HV3M_PowerManagement)
            cp "$file" pm/
            ;;
        RCH3M3M3NE3D3V3UV3H_DMAE)
            cp "$file" dmae/
            ;;
        RCH3M3M3NE3D3V3UV3H_Display)
            cp "$file" display/
            ;;
        RCH3M3M3NE3D3V3UV3H_GPIO)
            cp "$file" gpio/
            ;;
        RCH3M3M3NE3D3V3UV3H_I2C)
            cp "$file" i2c/
            ;;
        RCH3M3M3NE3D3V3UV3H_IPMMU)
            cp "$file" ipmmu/
            ;;
        RCH3M3M3NE3D3V3UV3H_MSIOF)
            cp "$file" msiof/
            ;;
        RCH3M3M3NE3D3V3UV3H_PWM)
            cp "$file" pwm/
            ;;
        RCH3M3M3NE3D3_Audio)
            cp "$file" audio/
            ;;
    esac
}

# Main loop to process all PDF files
for file in *.pdf; do
    if [[ $file == RENESAS_* ]]; then
        copy_file_to_folder "$file"
    fi
done





# Check if an argument is provided
if [ $# -eq 1 ]; then
    # Process only the specific PDF creation
    file_number=$1
    if (( file_number >= 1 && file_number <= $numberoffile )); then
        echo "Running pdf $file_number....."
        ./create_pdf_v3.sh $file_number
    else
        echo "Invalid file number. Please enter a number between 1 and 11."
        exit 1
    fi
else
    # Process all PDF creations
    for i in {1..$numberoffile}
    do
        echo "Running pdf $i....."
        ./create_pdf_v3.sh $i
    done
fi
