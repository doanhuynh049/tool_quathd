#!/bin/bash

# Reference
# ===
#
# 1/ Notes on creating PDF files
# ---
# Refer to the following guide
# https://renesasgroup.sharepoint.com/sites/REL-portal/relgmc/DocLib20/作成ガイド/ユーザーズマニュアル作成ガイド.aspx
#    + PDF Security: b-0011_20170324.pdf
#    + How to put PDF attributes: B-0020_20170324.pdf
#
# 2/ Automatic setup script for PDF security and PDF attribute (provided by Mr. Nagai)
# ---
# pdf_convert.sh
#
#
# Required packages
# ===
# This script requires pdftk, qpdf. To install them, execute below command:
# $ sudo apt install pdftk qpdf
#
# Because the security setting in accordance with the guideline is not able to be reflected by standard qpdf,
# you need use the custom version of qpdf attached at http://172.29.143.164:8080/attachments/download/65713/qpdf
# Furthermore, qpdf should be stored in the same folder with this script.
#
# How to use
# ===
# In create_pdf.sh, please check and update 2 array variables below:
#     + input_pdf array: Input PDF filenames. The final PDF will have same order as in this array
#     + title array: The corresponding title for above PDF files. To skip the title, put '' there
#
# ./create_pdf.sh <INPUT_DIR> <OUTPUT_PDF> <TITLE_NAME>
#
# Final PDF file will be put under current active directory, with
# - Merge all PDF files in defined order
# - Set up the PDF bookmarks
# - Set up the PDF properties
# - Set up the PDF atrribute
# - Set up the PDF security
#
# If you have any question, feel free to contact us via below emails:
#     + An Nguyen <an.nguyen.zc@rvc.renesas.com>
#     + Hai Nguyen Pham <hai.pham.ud@renesas.com>
# ---
# Ver.1.20181129
#   - Initial version
# Ver.2.20190131
#   - Add qpdf version check
# Ver.3.20190309
#   - Update qpdf version check function
#   - Add review scripts function (Optional)
#   - Update information for common input pdf files. Use their original name

### USER INPUT ###
INPUT_DIR1="/home/quathd/rvc/TaskDocument/pm/pdf_files"
INPUT_DIR2="/home/quathd/rvc/TaskDocument/thermal/pdf_files"
INPUT_DIR3="/home/quathd/rvc/TaskDocument/cmem/pdf_files"
INPUT_DIR4="/home/quathd/rvc/TaskDocument/display/pdf_files"
INPUT_DIR5="/home/quathd/rvc/TaskDocument/dmae/pdf_files"
INPUT_DIR6="/home/quathd/rvc/TaskDocument/gpio/pdf_files"
INPUT_DIR7="/home/quathd/rvc/TaskDocument/i2c/pdf_files"
INPUT_DIR8="/home/quathd/rvc/TaskDocument/ipmmu/pdf_files"
INPUT_DIR9="/home/quathd/rvc/TaskDocument/msiof/pdf_files"
INPUT_DIR10="/home/quathd/rvc/TaskDocument/pwm/pdf_files"
INPUT_DIR11="/home/quathd/rvc/TaskDocument/audio/pdf_files"

OUTPUT_PDF1="RENESAS_RCH3M3M3NE3D3V3UV3HV3M_PowerManagement_UME_v3.1.0.pdf"
OUTPUT_PDF2="RENESAS_RCH3M3M3NE3D3V3H_ThermalDriver_UME_v3.1.0.pdf"
OUTPUT_PDF3="RENESAS_RCH3M3M3NE3D3V3UV3HV3M_CMEM_UME_v3.1.0.pdf"
OUTPUT_PDF4="RENESAS_RCH3M3M3NE3D3V3UV3H_Display_UME_v3.1.0.pdf"
OUTPUT_PDF5="RENESAS_RCH3M3M3NE3D3V3UV3H_DMAE_UME_v3.1.0.pdf"
OUTPUT_PDF6="RENESAS_RCH3M3M3NE3D3V3UV3H_GPIO_UME_v3.1.0.pdf"
OUTPUT_PDF7="RENESAS_RCH3M3M3NE3D3V3UV3H_I2C_UME_v3.1.0.pdf"
OUTPUT_PDF8="RENESAS_RCH3M3M3NE3D3V3UV3H_IPMMU_UME_v3.1.0.pdf"
OUTPUT_PDF9="RENESAS_RCH3M3M3NE3D3V3UV3H_MSIOF_UME_v3.1.0.pdf"
OUTPUT_PDF10="RENESAS_RCH3M3M3NE3D3V3UV3H_PWM_UME_v3.1.0.pdf"
OUTPUT_PDF11="RENESAS_RCH3M3M3NE3D3_Audio_UME_v3.1.0.pdf"

TITLE_NAME1="R-Car Series Linux Interface Specification Power Management User’s Manual: Software"
TITLE_NAME2="R-Car Series Linux Interface Specification Device Driver Thermal Sensor User’s Manual: Software"
TITLE_NAME3="R-Car Series Linux Interface Specification Device Driver CMEM for Linux User’s Manual: Software"
TITLE_NAME4="R-Car Series Linux Interface Specification Device Driver Display User’s Manual: Software"
TITLE_NAME5="R-Car Series Linux Interface Specification Device Driver DMA Engine User’s Manual: Software"
TITLE_NAME6="R-Car Series Linux Interface Specification Device Driver GPIO  User’s Manual: Software"
TITLE_NAME7="R-Car Series Linux Interface Specification Device Driver I2C  User’s Manual: Software"
TITLE_NAME8="R-Car Series Linux Interface Specification Device Driver IPMMU  User’s Manual: Software"
TITLE_NAME9="R-Car Series Linux Interface Specification Device Driver IPMMU  User’s Manual: Software"
TITLE_NAME10="R-Car Series Linux Interface Specification Device Driver PWM  User’s Manual: Software"
TITLE_NAME11="R-Car Series Linux Interface Specification Device Driver Audio User’s Manual: Software"

INPUT_DIR="INPUT_DIR$1"
INPUT_DIR=${!INPUT_DIR}
echo $INPUT_DIR
OUTPUT_PDF="OUTPUT_PDF$1"
OUTPUT_PDF=${!OUTPUT_PDF}
TITLE_NAME="TITLE_NAME$1"
TITLE_NAME=${!TITLE_NAME}

input_pdf=(
	'01_Cover.pdf'
	'02_Notice.pdf'
	'03_TradeMark.pdf'
	'04_HowToUseThisManual.pdf'
	'05_TableOfContents.pdf'
	'06_Body.pdf'
	'07_RevisionHistory.pdf'
	'08_Colophon.pdf'
	'09a_en_AddressList.pdf'
	'09b_ja_AddressList.pdf'
	'10_BackCover.pdf'
)

title=(
	'Cover'
	'Notice'
	'Trade Mark'
	'How To Use This Manual'
	'Table Of Contents'
	'Body'
	'Revision History'
	'Colophon'
	'Address List'
	''
	'Back Cover'
)

### GLOBAL VAR ###
WORKING_DIR="$PWD/tmp"
pdf_data_info="$WORKING_DIR/out.info"
qpdf_MD5SUM="d63a4758fe4a4317e2bc1ec723c367b9"
qpdf_PATH=$(dirname $(readlink -f $0))/qpdf
review_script="doc_review_fc31071.py"

### PLEASE DO NOT MODIFY THE SCRIPT BELOW ###

### FUNCTION ###

increase_bookmark_level() {
pdftk $1 dump_data output $2
sed -i -- "s/BookmarkLevel: 6/BookmarkLevel: 7/g" $2
sed -i -- "s/BookmarkLevel: 5/BookmarkLevel: 6/g" $2
sed -i -- "s/BookmarkLevel: 4/BookmarkLevel: 5/g" $2
sed -i -- "s/BookmarkLevel: 3/BookmarkLevel: 4/g" $2
sed -i -- "s/BookmarkLevel: 2/BookmarkLevel: 3/g" $2
sed -i -- "s/BookmarkLevel: 1/BookmarkLevel: 2/g" $2
}

parse_bookmark() {
cat<<EOS > $pdf_data_info
BookmarkBegin
BookmarkTitle: $@
BookmarkLevel: 1
BookmarkPageNumber: 1
EOS
}

create_bookmark() {
cd $1

for i in ${!input_pdf[@]}
do
	if [ "X${title[$i]}" != "X" ]; then
		parse_bookmark "${title[$i]}"
		if [ "X${title[$i]}" == "XBody" ]; then
			increase_bookmark_level "${input_pdf[$i]}" $2/tmp.info
			cat $2/tmp.info >> $pdf_data_info
		fi
		pdftk "${input_pdf[$i]}" update_info $pdf_data_info output $2/out{`printf "%02.0f" ${i}`}.pdf
	else
		pdftk "${input_pdf[$i]}" output $2/out{`printf "%02.0f" ${i}`}.pdf
	fi
done

cd -
}

merge_pdf() {
pdftk $1 output $2
}

update_pdf_property() {
pdftk $1 dump_data > $pdf_data_info

cat <<EOS >> $pdf_data_info
InfoBegin
InfoKey: Title
InfoValue: ${TITLE_NAME}
InfoBegin
InfoKey: Author
InfoValue: Renesas Electronics Corporation
InfoBegin
InfoKey: Subject
InfoValue:
InfoBegin
InfoKey: Creator
InfoValue:
InfoBegin
InfoKey: Producer
InfoValue:
InfoBegin
InfoKey: Keywords
InfoValue:
EOS

pdftk $1 update_info $pdf_data_info output $2
}

apply_pdf_security() {
# Apply PDF security
$qpdf_PATH \
  --linearize \
  --encrypt '' Renesas_Doc_Protection 128 \
  --accessibility=y \
  --extract=y \
  --print=full \
  --modify=annotate \
  --use-aes=y \
  -- \
  $1 $2
}

check_qpdf_version() {
md5sum $1 | grep -q $2

if [ "X$?" = "X1" ]; then
	echo "Not supported QPDF version!"
	echo "Please use qpdf version 6.0.0 instead"
	echo "(MD5SUM: $qpdf_MD5SUM)"
	exit 1
fi
}

doc_review() {
python $1 --path $2
}

### MAIN ###

mkdir -p $WORKING_DIR

check_qpdf_version "$qpdf_PATH" "$qpdf_MD5SUM"
create_bookmark "$INPUT_DIR" "$WORKING_DIR"
merge_pdf "$WORKING_DIR/*.pdf" "$WORKING_DIR/Binder.pdf"
update_pdf_property "$WORKING_DIR/Binder.pdf" "$WORKING_DIR/tmp.pdf"
apply_pdf_security "$WORKING_DIR/tmp.pdf" "$OUTPUT_PDF"
# doc_review "$review_script" `pwd`

rm -rf $WORKING_DIR
