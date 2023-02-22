
echo 1024 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
dpdk-hugepages.py -s

modprobe uio_pci_generic
dpdk-devbind.py -b uio_pci_generic 0000:06:00.0 --force
dpdk-devbind -s
