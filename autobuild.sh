age:
#   ./autobuild.sh [branch_name]
#
#   If no branch name is provided, the script will show a list of all branches.
#   If a branch name is provided, the script will switch to that branch, update it to the latest version,
#   show the branch status, and prompt the user to build the project.

function build(){
    # List all revisions in the specified branch
    # echo "List all revisions in branch "
    # git log --oneline $1
    # # Prompt the user to select a revision to build
    # echo -n "which revision do you want to build (enter commit hash): "
    # read revision
    # # If the specified revision is not in the current branch, use the current HEAD
    # if ! git branch --contains "$revision" &>/dev/null; then
    #     revision= git rev-parse HEAD
    # fi
    # echo "Git revision: $revision"
    # # Checkout the specified revision
    # git checkout $revision
    # Clean the project
    echo -n "Cleaning project..."
    make clean
    # Build the project
    echo -n "Building project..."
    make all
    # Create the image
    echo $PATH
    echo -n "Creating image..."
    mkappfsimg
}
function mkappfsimg(){
    # scripted by HOSHIBA

    # remove appfs.rel and appfs.rel.spfs.img if they are
    APPFSREL="appfs.rel"
    APPFSIMG="appfs.rel.sqfs.img"

    if [ -e $APPFSREL ]; then
        rm -fr $APPFSREL
    fi
    if [ -e $APPFSIMG ]; then
            rm -fr $APPFSIMG
    fi

    # copy
    echo "copy rel to appfs.rel start"
    cp -ar rel appfs.rel
    echo "copy finished"

    # strip
    echo "strip start"
    aarch64-poky-linux-strip appfs.rel/bin/HMIManager
    aarch64-poky-linux-strip appfs.rel/lib/libcctvrec_mmc.so.0.0.1
    aarch64-poky-linux-strip appfs.rel/lib/liblog.so
    aarch64-poky-linux-strip appfs.rel/lib/libsrr.so
    aarch64-poky-linux-strip appfs.rel/maintenance/cctv-test
    echo "strip finished"

    # commress into appfs.rel.sqfs.img
    echo "making squash image start"
    /opt/host/bin/mkfsimg.sh appfs.rel
    echo "making squash image finished"
}
function flash(){
    echo "Flash with following commands:"
    echo "cd ../tmp/dis"
    echo "update_dis.sh appfs B"
    echo "devmem 0xc8000024 16 1"
    echo "cd ../tmp/dis && update_dis.sh appfs B && devmem 0xc8000024 16 1"

    sshpass -p '##u3_rec' ssh 'root@192.168.2.100'
    # if [ -e "tmp/dis/appfs.rel.sqfs.img" ]; then
    #     echo "Image available..."
    #     echo -n "Do you want to flash diplay? (y/n): "
    #     read flash
    #     if [ "$flash" == "y" ]; then
    #         ll
    #     update_dis.sh appfs B
    #     devmem 0xc8000024 16 1
    #     else
    #         exit;
    #     fi
    # fi
}
function Cp_Image_to_Root(){
    # Copy the image to the specified location, and prompt the user to flash the display
    echo -n "Do you want to copy image to storing (root@192.168.2.100:/tmp/dis)? (y/n): "
    read image
     while true; do
        case $image in
            [Yy]* ) echo "Copying..."
                    sshpass -p '##u3_rec' ssh 'root@192.168.2.100' 'mkdir -p /tmp/dis'
                    sshpass -p '##u3_rec' scp -r -O appfs.rel.sqfs.img root@192.168.2.100:/tmp/dis;
                    flash;
                    break;;
            [Nn]* ) echo "Exiting..."; break;;
            * ) echo "Please enter y or n"; read image;;
        esac
    done
}

if [ -z "$1" ]; then
    # If no branch name is provided, show a list of all branches
    git branch -a
    echo "choose branch to flash"
else
    # Switch to the specified branch, update to the latest version, and show status
    git show-branch "$s1"
    git checkout "$1"
    git pull        # to pull latest version of branch
    echo "show branch used"
    git branch      # to show branch used
    echo "branch status"
    git status
    # Check for changes in the working tree
    if ! git diff-index --quiet HEAD --; then
        echo -n "detected changes, choose option 1.commit 2.clean 3.skip ? "
        read clean
        case $clean in
            1* ) git add .;
                    git commit -m "update";
                    git push --set-upstream origin $1; break;;
            2* ) git clean -fdx;;
            3* ) break;;
            * ) echo -n "Please enter 1 2 or 3: "; read clean; sleep 1;;
        esac
    fi


    source ~/.bashrc
    echo -n "Do you want to build mkappfsimg.sh? (y/n): "
    read confirm

    while true; do
        case $confirm in
            [Yy]* ) build $revision; break;;
            [Nn]* ) echo "Exiting build..."; break;;
            * ) echo -n "Please enter y or n: "; read confirm; sleep 1;;
        esac
    done

    # Check the image size and prompt the user to continue or abort
    if [ -e "appfs.rel.sqfs.img" ]; then
        echo "Image exist"
        echo -n "Image size:"
        ls -lh appfs.rel.sqfs.img
        size=$(du -k appfs.rel.sqfs.img | awk '{print $1}')
        if [[ $size -gt 8100 || $size -lt 7000 ]]; then
            exit;       # pass condition 
            echo -n "The image size is $size KB. Do you want to continue? (y/n): "
            read confirm_size
            case ${confirm_size:0:1} in
                [Yy]* ) Cp_Image_to_Root;
                        break;;
                [Nn]* ) echo "Exiting..."; 
                        break;;
                * ) echo -n "Please enter y or n: "; read confirm_size; sleep 1;;
            esac
        else
            echo "The image size is $size KB. It is OKIE!"
            Cp_Image_to_Root
        fi
    else
        echo "Image file not found."
        exit;
    fi
    

    # if [ "$image"  == "y" ]; then

    # else
    #     exit;
    # fi
    git switch -
fi
