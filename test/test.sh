#!/bin/bash
pwd=`pwd`
down_dir=$pwd/down/
down()
{
    $pwd/bin/http_down_main -u http://xp6.xitongxz.net/201404/YLMF_GhostXP_SP3_YN2014_04.iso  -d $down_dir -t 20
}
clean()
{
    rm -rf  $down_dir/*
}
case "$1" in
    down)
        down
        ;;
    clean)
        clean
        ;;
    *)
        echo $"Usage:$0{down|clean}"
esac
