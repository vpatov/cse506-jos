#!/bin/bash

COMMAND=$0
MY_DIR=`dirname $0`
CP_KEY=$MY_DIR/handin-key
USER_KEY=$HOME/.ssh/id_rsa

# run some sanity checks on the input file: stat, size, etc.
FILE=$1
FILE_NAME=`basename $FILE`

# get the netid
source $MY_DIR/conf/user.mk
if [ "$NETID" == "" ] || [ "$NETID" == "your-netid" ]; then
    echo "!!! Please enter your NetID in conf/user.mk"
    exit 1
fi

# copy the file
NEW_FILE_NAME=$FILE_NAME=`date +%F=%T`
RES=`ssh -i $CP_KEY -p 130 nhonarmand@130.245.30.37 "$NETID" "$NEW_FILE_NAME" < $FILE`

# check the transfer success
MD5=`md5sum -b < $FILE` 
if [ "$RES" != "$MD5" ]; then
    echo "ERROR! transfer failed."
else
    echo "Congratulations! Your file was submitted successfully."
fi
