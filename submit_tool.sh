#!/bin/sh

ChangeID=$(git log -1 | grep "Change-Id:" |  sed 's/^[ \t]*//g')
Signed=$(git log -1 | grep "Signed-off-by:" |  sed 's/^[ \t]*//g')

git log -1 | sed '/Change-Id:/,$d' | sed '/Signed-off-by/,$d' |  sed '/SourceCode:/,$d' |  sed '/media_module version:/,$d' | sed -n '/^$/,$p' |  sed '/^$/d' | sed 's/^[ \t]*//g' > tmp.txt

#echo "media_module version:" >> tmp.txt
#echo $(./version_control.sh) | awk '{print $1}' | awk -F [=] '{print $2}'  >> tmp.txt

if [ "0$(git log --stat -1 | grep "video_ucode.bin")" != "0" ]; then
echo "SourceCode:
ucode:" >> tmp.txt
	./firmware/checkmsg ./firmware/video_ucode.bin | grep ver | awk '{print $3}' | sed 's/v//g' > version.txt
	./firmware/checkmsg ./firmware/video_ucode.bin  | grep -A 5 "change id history:" >> version.txt
	chmod 644 version.txt
	./firmware/checkmsg ./firmware/video_ucode.bin | grep ver | awk '{print $3}' | sed 's/v//g' >> tmp.txt
	cat version.txt | sed -n '3,7p' >> tmp.txt

	git add version.txt
fi

echo "" >> tmp.txt
echo ${ChangeID} >>  tmp.txt
echo ${Signed} >>  tmp.txt


git status
cat tmp.txt | xargs -0 git commit --amend -m

rm -f tmp.txt
