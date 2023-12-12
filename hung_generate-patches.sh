#!/bin/bash
file=$1
lines=$(wc -l $file)

total_steps=$()
update_progress() {
  local progress=$((100 * $1 / $total_steps))
  printf "\r[%-50s] %d%%" $(printf "=%.0s" {1..$progress}) $progress
}

# Check if the commit IDs file exists
if [ ! -f "patch.diff" ]; then
  echo "Commit IDs file 'patch.diff' not found."
  exit 1
fi

# Create a directory to store the patch files
mkdir -p patches
patch_number=0
# Read commit IDs from the file and generate patch files
while IFS= read -r commit_id; do
  # Check if the commit ID is valid
  if ! git rev-parse --quiet --verify "$commit_id" > /dev/null; then
    echo "Invalid commit ID: $commit_id" >> patches/invalid-commit-id.log
    continue
  fi
  
  # Generate the patch file for the commit
  
  git format-patch -1 --stdout "$commit_id" > patches/${patch_number}_${commit_id}.patch
  ((patch_number++))
  
  update_progress "$patch_number"
done < "patch.diff"
echo "Patch number: $patch_number" > patches/patch_number.log
echo -e ""
echo "Patch generation complete."

