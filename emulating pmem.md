## Emulating PMEM with DRAM

1. edit /etc/default/grub and uncomment the memmap line
    - change 4G!4G to 1G!4G to make it 1GB of pmem
    - memmap=512M!4G means 512 mb of pmem from the 4GB starting point
    - check `sudo dmesg | grep BIOS-e820` for usable ranges

2. `sudo update-grub2`

3. reboot

4. `sudo fdisk -l /dev/pmem0` to check if it worked
    
    - `sudo cat /proc/iomem` or `sudo dmesg | grep user` 
    - to check if persistent memory is allocated a range

5. check if its in DAX mode with `sudo ndctl list -u`

    - `sudo ndctl create-namespace -f -e namespace0.0 --mode=fsdax` to switch to DAX

6. `sudo mkfs.ext4 /dev/pmem0` to create the file

    - this might mount it automatically so do `sudo mount -v | grep pmem`
    - if its there, use `sudo umount <mount path>` to unmount it from the automatic mount path it chooses
    - usually /media/...
    

7. then `sudo mount -o dax /dev/pmem0 /mnt/pmem` to mount it as DAX filesystem

    - /mnt/pmem is the path where you'll make the pmem pools under
    - can be whatever path you want but create it first `mkdir /mnt/pmem`
    - might have to change the permissions on the folder: `sudo chmod 0777 /mnt/pmem`

8. use `sudo mount -v | grep dax` to check if the mount worked with DAX

9. check for mount errors with `sudo dmesg | grep pmem` 
