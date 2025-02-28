#!/usr/bin/env bash

files=$(ls ./*.png)
for file in $files; do
  if [[ $file =~ [0-9]~color\.png$ ]]; then
    newfile="${file%~color.png}~bw.png"
    ffmpeg -i "$file" -filter_complex "[0]split=2[bg][fg];[bg]drawbox=c=white@1:replace=1:t=fill[bg];[bg][fg]overlay=format=auto[out1];[out1]negate" -pix_fmt monob "$newfile" -y
  fi
done
