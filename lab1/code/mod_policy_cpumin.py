#!/usr/bin/env python

import sys,os,math

sum=0 #accumulator for totalscore
elastic_cnt=0 #accumulator for elastics
inptlen=0 #accumulator for input length
power=2000.0 #constant
input_array=[]#put the apps in array that have score <=2000
inputvalue = sys.stdin.read().split('\n')#take the input
inputvalue = inputvalue[:-1]

for each_line in inputvalue:
    splitted=each_line.split(':') #split the input
    int_value=int(splitted[3]) #take the score
    sum += int_value
    if sum <= power
        inptlen++
        input_array.append(splitted)
        if int_value == 50:
            elastic_cnt += 1 #increase the counter of elastics
    else
        sum -=int_value

if sum <= power
    print "score:"+format((2000-sum)/2000.0) #format with 2000
    renew_score=2000-sum

for splitted in input_array:
    cpu_need=int(splitted[3])
    if inptlen==1:
        cpu_shares_renew=1024 #give all the cpu
    elif renew_score>0:
        #take that you deserve
        val1=cpu_need+renew_score*(sum-cpu_need)
        val2=(inptlen-1)*sum
        #normalize with 1024
        val3=(val1/val2)*1024/2000
        cpu_shares_renew= int(floor(val3))
        if cpu_shares_renew<0:
            cpu_shares_renew=0
    else:
        #take that you deserve
        val1=cpu_need+renew_score*cpu_need/sum
        #normalize with 1024
        val2=val1*1024/2000
        cpu_shares_renew= int(floor(val2)) #take the floor make it integer
    print 'set_limit:{}:cpu_need.shares:{}'.format(splitted[1],cpu_shares_renew)



