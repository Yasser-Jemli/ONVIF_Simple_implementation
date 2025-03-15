#!/bin/bash 
# This script is used to test the onvif discovery service code 
# Author : Yasser JEMLI 
# Date : 15 mars 2025 


CURDIR=$(pwd)
EMULATOR_DIR=$CURDIR/happytime-multi-onvif-server
make clean 
make all

read -p "Do you want to run the onvif_discvery test ? (y/n)" choice
case "$choice" in 
  y|Y ) 
    echo "Running the onvif_discovery test"
    cd $EMULATOR_DIR
    ./start.sh & 
    cd $CURDIR/build
    sleep 2 
    ./onvif_discovery
    ;;
  n|N ) 
    echo "You chose to exit"
    ;;
  * ) 
    echo "Invalid choice"
    ;;
esac