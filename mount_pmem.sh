#!/bin/bash
if [[ $# -eq 0 ]]; then
  echo "usage $0 <mount_path>"
  exit 0
fi

if [ ! -d "$1" ]; then
  mkdir -p "$1"
fi

sudo mkfs.ext4 /dev/pmem0;
sudo mount -o dax /dev/pmem0 "$1";
sudo chmod 0777 "$1";