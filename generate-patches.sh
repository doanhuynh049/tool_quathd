#!/bin/bash

#check file exist
update_progress(){
	local progress=$((100 * $1 / $total_steps))
	printf "\r[%-50s] %d%%" $(printf "=%.0s" {1..$progress}) $progress
}
count_step(){
	lines=$(wc -l $file)
	number_of_lines=$(cut -d " " -f 1 <<< "$lines")
	total_steps=$number_of_lines
	echo -n "Total steps: "
	echo $total_steps
}
handle_commit(){
	rm -rf patches
	mkdir -p patches
	patch_number=0
	while IFS= read -r commit_id;do
		if ! git rev-parse --quiet --verify $commit_id > /dev/null; then
			echo "Invalid commit ID: $commit_id" >>patches/invalid-commit-id.log
			continue
		fi
		#Generate the patch file for the commit
		git format-patch -1 --stdout "$commit_id" > patches/${patch_number}_${commit_id}.patch
		((patch_number++))
		update_progress "$patch_number"
	done < $file
	echo "Patch number: $patch_number" > patches/patch_number.log
	echo -e ""
	echo "Patch generation complete."	

}
#git log 33069919e2dce440d3b8cd101b18f37bb35bdddf..452163c75612e4161099c0bf5178fd0cf60e2cad --oneline | cut -d " " -f 1 > patch.diff
if [ -f $1 ]; then
	echo "File exist"
	touch reverted_patch.diff
	tac $1 > reverted_patch.diff
	cat reverted_patch.diff
	file=$reverted_patch.diff 
	count_step
	handle_commit
else
	echo "Commit ID file is not found"
fi



