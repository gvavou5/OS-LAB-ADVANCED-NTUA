#!/usr/bin/env python

import sys
import os

#define the right paths for each mode
path_stdr="/sys/fs/cgroup/cpu/"
path_tasks="/tasks"
path_cpushares="/cpu.shares"

# take the input, split it and take each part and use it as you have to
for input in sys.stdin:
    splitted = line.split(':',6)
    if splitted[0]=="create":
        #create the right path
        path =path_stdr+splitted[1]+"/"+splitted[3]
        os.system("mkdir "+path)
    elif splitted[0]=="remove":
        #create the right path
        path =path_stdr+splitted[1]+"/"+splitted[3]
        os.system("rmdir "+path)
    elif splitted[0]=="add":
        #create the right path
        path =path_stdr+splitted[1]+"/"+splitted[3]+path_tasks
        os.system("echo "+splitted[4]+" > "+path )#pid=splitted[4]
    elif splitted[0]=="set_limit":
        #create the right path
        path =path_stdr+splitted[1]+"/"+splitted[3]+path_cpushares
        os.system("echo "+splitted[5]+" > "+path ) #value=splitted[5]
