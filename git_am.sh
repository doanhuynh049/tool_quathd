#!/bin/bash

PATCH_DIR="../linux-bsp/patches/"
PATCH_NUM=1
STOP_NUM=866
# Check if the patch directory exists
if [ ! -d "$PATCH_DIR" ]; then
  echo "Patch directory not found: $PATCH_DIR"
  exit 1
fi
PATCH_PASS_COUNTER=0
# Apply patches using a loop
while [ $PATCH_NUM -le $STOP_NUM ]; do
  patch_file="$PATCH_DIR${PATCH_NUM}_*.patch"
  if git am $patch_file; then
    PATCH_PASS_COUNTER=$((PATCH_PASS_COUNTER + 1))
  else
    echo "git am failed for patch number $PATCH_NUM"
    echo -e $PATCH_NUM >> patch-failed.log
    git am --abort
  fi
  PATCH_NUM=$((PATCH_NUM + 1))
done
echo $PATCH_PASS_COUNTER

