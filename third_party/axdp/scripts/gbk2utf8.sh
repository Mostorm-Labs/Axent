#!/usr/bin/env zsh
ToBeConverted=$1
ConvertedTmp=${ToBeConverted}.tmp
echo -e "Convert gbk-encode file \033[36m[${ToBeConverted}]\033[0m to utf8-encode"
iconv -f gbk -t utf-8 "${ToBeConverted}" > "${ConvertedTmp}"
#if iconv -f gbk -t utf-8 "${ToBeConverted}" > "${ConvertedTmp}";
#then
if [ $? -ne 0 ]; then
  echo "Convert failed."
  rm "${ConvertedTmp}"
else
  echo "Convert successfully."
  rm "${ToBeConverted}"
  mv "${ConvertedTmp}" "${ToBeConverted}"
fi